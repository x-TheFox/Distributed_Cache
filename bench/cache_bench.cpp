#include "core/concurrent_store.hpp"
#include "core/request_coalescer.hpp"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cmath>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using cache::core::ConcurrentStore;
using Clock = std::chrono::steady_clock;

namespace {
struct Config {
  size_t ops = 1000000;
  size_t key_space = 2000;
  double read_ratio = 0.8;
  size_t stripes = 8;
};

struct ScenarioResult {
  std::string name;
  size_t requests;
  double ops_per_sec;
  double p50_ms;
  double p99_ms;
  double error_rate;
  double coalescing_hit_ratio;
  double duplicate_backend_hits;
  std::string status;
};

struct ScenarioSpec {
  std::string name;
  size_t requests;
};

struct ScenarioMatrixResult {
  std::vector<ScenarioSpec> scenarios;
  std::string error;
};

struct CoalescingBenchmarkResult {
  double duplicate_backend_hits = 0.0;
  double coalescing_hit_ratio = 0.0;
};

CoalescingBenchmarkResult MeasureCoalescingBehavior(bool enabled, size_t bursts,
                                                    size_t concurrency) {
  cache::core::RequestCoalescer coalescer;
  std::atomic<size_t> backend_hits{0};
  std::atomic<size_t> total_requests{0};

  for (size_t burst = 0; burst < bursts; ++burst) {
    std::barrier start_gate(static_cast<std::ptrdiff_t>(concurrency));
    std::vector<std::thread> workers;
    workers.reserve(concurrency);
    std::string key = "coalescing_key_" + std::to_string(burst);
    for (size_t i = 0; i < concurrency; ++i) {
      workers.emplace_back([&, key]() {
        start_gate.arrive_and_wait();
        total_requests.fetch_add(1, std::memory_order_relaxed);
        auto work = [&backend_hits]() {
          backend_hits.fetch_add(1, std::memory_order_relaxed);
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
          return std::string("payload");
        };
        if (enabled) {
          auto future = coalescer.Execute(key, work);
          (void)future.get();
        } else {
          (void)work();
        }
      });
    }
    for (auto& worker : workers) {
      worker.join();
    }
  }

  size_t hits = backend_hits.load(std::memory_order_relaxed);
  size_t requests = total_requests.load(std::memory_order_relaxed);
  size_t expected_unique_hits = bursts;
  size_t duplicates =
      hits > expected_unique_hits ? hits - expected_unique_hits : 0;
  double coalescing_hit_ratio =
      requests > 0
          ? static_cast<double>(requests - hits) / static_cast<double>(requests)
          : 0.0;
  return {static_cast<double>(duplicates), coalescing_hit_ratio};
}

std::string JsonEscapeString(std::string_view input) {
  std::string escaped;
  escaped.reserve(input.size());
  for (unsigned char ch : input) {
    switch (ch) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (ch < 0x20) {
          constexpr char kHex[] = "0123456789abcdef";
          escaped += "\\u00";
          escaped.push_back(kHex[ch >> 4]);
          escaped.push_back(kHex[ch & 0x0F]);
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  return escaped;
}

class JsonReader {
 public:
  explicit JsonReader(std::string_view input) : input_(input) {}

  void SkipWhitespace() {
    while (pos_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
  }

  bool Consume(char expected) {
    SkipWhitespace();
    if (pos_ >= input_.size() || input_[pos_] != expected) {
      return false;
    }
    ++pos_;
    return true;
  }

  bool ParseString(std::string& out) {
    SkipWhitespace();
    if (pos_ >= input_.size() || input_[pos_] != '"') {
      return false;
    }
    ++pos_;
    out.clear();
    while (pos_ < input_.size()) {
      char ch = input_[pos_++];
      if (ch == '"') {
        return true;
      }
      if (ch == '\\') {
        if (pos_ >= input_.size()) {
          return false;
        }
        char esc = input_[pos_++];
        switch (esc) {
          case '"':
          case '\\':
          case '/':
            out.push_back(esc);
            break;
          case 'b':
            out.push_back('\b');
            break;
          case 'f':
            out.push_back('\f');
            break;
          case 'n':
            out.push_back('\n');
            break;
          case 'r':
            out.push_back('\r');
            break;
          case 't':
            out.push_back('\t');
            break;
          case 'u':
            if (!ParseUnicodeEscape(out)) {
              return false;
            }
            break;
          default:
            return false;
        }
      } else {
        if (static_cast<unsigned char>(ch) < 0x20) {
          return false;
        }
        out.push_back(ch);
      }
    }
    return false;
  }

  bool ParseScenarioArray(std::vector<ScenarioSpec>& scenarios,
                          size_t default_requests) {
    if (!Consume('[')) {
      return false;
    }
    SkipWhitespace();
    if (Consume(']')) {
      return true;
    }
    while (true) {
      if (!ParseScenarioObject(scenarios, default_requests)) {
        return false;
      }
      SkipWhitespace();
      if (Consume(']')) {
        return true;
      }
      if (!Consume(',')) {
        return false;
      }
    }
  }

  bool SkipValue() {
    SkipWhitespace();
    if (pos_ >= input_.size()) {
      return false;
    }
    char ch = input_[pos_];
    if (ch == '"') {
      std::string discard;
      return ParseString(discard);
    }
    if (ch == '{') {
      return SkipObject();
    }
    if (ch == '[') {
      return SkipArray();
    }
    if (ch == 't' && input_.substr(pos_, 4) == "true") {
      pos_ += 4;
      return true;
    }
    if (ch == 'f' && input_.substr(pos_, 5) == "false") {
      pos_ += 5;
      return true;
    }
    if (ch == 'n' && input_.substr(pos_, 4) == "null") {
      pos_ += 4;
      return true;
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
      ++pos_;
      while (pos_ < input_.size()) {
        char next = input_[pos_];
        if (std::isdigit(static_cast<unsigned char>(next)) || next == '.' ||
            next == 'e' || next == 'E' || next == '-' || next == '+') {
          ++pos_;
        } else {
          break;
        }
      }
      return true;
    }
    return false;
  }

  bool AtEnd() {
    SkipWhitespace();
    return pos_ >= input_.size();
  }

 private:
  bool ParseUnsigned(size_t& out) {
    SkipWhitespace();
    if (pos_ >= input_.size() ||
        !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
      return false;
    }
    size_t value = 0;
    while (pos_ < input_.size() &&
           std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
      unsigned digit = static_cast<unsigned>(input_[pos_] - '0');
      if (value >
          (std::numeric_limits<size_t>::max() - digit) / static_cast<size_t>(10)) {
        return false;
      }
      value = value * static_cast<size_t>(10) + digit;
      ++pos_;
    }
    if (pos_ < input_.size()) {
      char next = input_[pos_];
      if (next == '.' || next == 'e' || next == 'E' || next == '-' ||
          next == '+') {
        return false;
      }
    }
    out = value;
    return true;
  }

  bool SkipArray() {
    if (!Consume('[')) {
      return false;
    }
    SkipWhitespace();
    if (Consume(']')) {
      return true;
    }
    while (true) {
      if (!SkipValue()) {
        return false;
      }
      SkipWhitespace();
      if (Consume(']')) {
        return true;
      }
      if (!Consume(',')) {
        return false;
      }
    }
  }

  bool SkipObject() {
    if (!Consume('{')) {
      return false;
    }
    SkipWhitespace();
    if (Consume('}')) {
      return true;
    }
    while (true) {
      std::string key;
      if (!ParseString(key)) {
        return false;
      }
      if (!Consume(':')) {
        return false;
      }
      if (!SkipValue()) {
        return false;
      }
      SkipWhitespace();
      if (Consume('}')) {
        return true;
      }
      if (!Consume(',')) {
        return false;
      }
    }
  }

  bool ParseScenarioObject(std::vector<ScenarioSpec>& scenarios,
                           size_t default_requests) {
    if (!Consume('{')) {
      return false;
    }
    bool found_name = false;
    size_t requests = default_requests;
    std::string name;
    SkipWhitespace();
    if (Consume('}')) {
      return false;
    }
    while (true) {
      std::string key;
      if (!ParseString(key)) {
        return false;
      }
      if (!Consume(':')) {
        return false;
      }
      if (key == "name") {
        if (!ParseString(name)) {
          return false;
        }
        found_name = true;
      } else if (key == "requests") {
        size_t parsed_requests = 0;
        if (!ParseUnsigned(parsed_requests)) {
          return false;
        }
        requests = parsed_requests;
      } else {
        if (!SkipValue()) {
          return false;
        }
      }
      SkipWhitespace();
      if (Consume('}')) {
        if (!found_name) {
          return false;
        }
        scenarios.push_back({name, requests});
        return true;
      }
      if (!Consume(',')) {
        return false;
      }
    }
  }

  std::string_view input_;
  size_t pos_ = 0;

  static bool AppendUtf8(uint32_t codepoint, std::string& out) {
    if (codepoint <= 0x7F) {
      out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
      out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
      out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
      out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      return false;
    }
    return true;
  }

  bool ParseUnicodeEscape(std::string& out) {
    uint32_t codepoint = 0;
    if (!ParseHex4(codepoint)) {
      return false;
    }
    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
      if (pos_ + 6 > input_.size()) {
        return false;
      }
      if (input_[pos_] != '\\' || input_[pos_ + 1] != 'u') {
        return false;
      }
      pos_ += 2;
      uint32_t low = 0;
      if (!ParseHex4(low)) {
        return false;
      }
      if (low < 0xDC00 || low > 0xDFFF) {
        return false;
      }
      codepoint =
          0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
    } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
      return false;
    }
    return AppendUtf8(codepoint, out);
  }

  bool ParseHex4(uint32_t& codepoint) {
    if (pos_ + 4 > input_.size()) {
      return false;
    }
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      char ch = input_[pos_++];
      value <<= 4;
      if (ch >= '0' && ch <= '9') {
        value |= static_cast<uint32_t>(ch - '0');
      } else if (ch >= 'a' && ch <= 'f') {
        value |= static_cast<uint32_t>(10 + ch - 'a');
      } else if (ch >= 'A' && ch <= 'F') {
        value |= static_cast<uint32_t>(10 + ch - 'A');
      } else {
        return false;
      }
    }
    codepoint = value;
    return true;
  }
};

bool ParseScenarioMatrix(std::string_view contents, size_t default_requests,
                         std::vector<ScenarioSpec>& scenarios) {
  JsonReader reader(contents);
  if (!reader.Consume('{')) {
    return false;
  }
  bool found_scenarios = false;
  reader.SkipWhitespace();
  if (reader.Consume('}')) {
    return false;
  }
  while (true) {
    std::string key;
    if (!reader.ParseString(key)) {
      return false;
    }
    if (!reader.Consume(':')) {
      return false;
    }
    if (key == "scenarios") {
      if (found_scenarios) {
        return false;
      }
      found_scenarios = true;
      if (!reader.ParseScenarioArray(scenarios, default_requests)) {
        return false;
      }
    } else {
      if (!reader.SkipValue()) {
        return false;
      }
    }
    reader.SkipWhitespace();
    if (reader.Consume('}')) {
      break;
    }
    if (!reader.Consume(',')) {
      return false;
    }
  }
  if (!found_scenarios || scenarios.empty()) {
    return false;
  }
  return reader.AtEnd();
}

bool SumScenarioRequests(const std::vector<ScenarioSpec>& scenarios,
                         size_t& total) {
  total = 0;
  for (const auto& scenario : scenarios) {
    if (scenario.requests >
        (std::numeric_limits<size_t>::max() - total)) {
      return false;
    }
    total += scenario.requests;
  }
  return true;
}

std::vector<size_t> DistributeScenarioRequests(
    const std::vector<ScenarioSpec>& scenarios, size_t total_ops) {
  std::vector<size_t> requests(scenarios.size(), 0);
  if (scenarios.empty() || total_ops == 0) {
    return requests;
  }
  long double weight_sum = 0.0L;
  for (const auto& scenario : scenarios) {
    weight_sum += static_cast<long double>(scenario.requests);
  }
  if (weight_sum <= 0.0L) {
    size_t base = total_ops / scenarios.size();
    size_t remainder = total_ops % scenarios.size();
    for (size_t i = 0; i < scenarios.size(); ++i) {
      requests[i] = base + (i < remainder ? 1 : 0);
    }
    return requests;
  }
  struct FractionalPart {
    size_t index;
    long double fraction;
  };
  std::vector<FractionalPart> fractional_parts;
  fractional_parts.reserve(scenarios.size());
  size_t allocated = 0;
  for (size_t i = 0; i < scenarios.size(); ++i) {
    long double quota =
        (static_cast<long double>(total_ops) *
         static_cast<long double>(scenarios[i].requests)) /
        weight_sum;
    long double base_ld = std::floor(quota);
    if (base_ld < 0.0L) {
      base_ld = 0.0L;
    }
    if (base_ld > static_cast<long double>(total_ops)) {
      base_ld = static_cast<long double>(total_ops);
    }
    size_t base = static_cast<size_t>(base_ld);
    if (base > total_ops - allocated) {
      base = total_ops - allocated;
    }
    requests[i] = base;
    allocated += base;
    long double fraction = quota - static_cast<long double>(base);
    if (fraction < 0.0L) {
      fraction = 0.0L;
    }
    fractional_parts.push_back({i, fraction});
  }
  if (allocated == total_ops) {
    return requests;
  }
  if (allocated < total_ops) {
    size_t remainder = total_ops - allocated;
    std::sort(
        fractional_parts.begin(), fractional_parts.end(),
        [](const FractionalPart& a, const FractionalPart& b) {
          if (a.fraction == b.fraction) {
            return a.index < b.index;
          }
          return a.fraction > b.fraction;
        });
    for (size_t i = 0; i < remainder; ++i) {
      requests[fractional_parts[i % fractional_parts.size()].index] += 1;
    }
    return requests;
  }
  size_t excess = allocated - total_ops;
  std::sort(
      fractional_parts.begin(), fractional_parts.end(),
      [](const FractionalPart& a, const FractionalPart& b) {
        if (a.fraction == b.fraction) {
          return a.index < b.index;
        }
        return a.fraction < b.fraction;
      });
  for (size_t i = 0; i < excess; ++i) {
    size_t idx = fractional_parts[i % fractional_parts.size()].index;
    if (requests[idx] > 0) {
      requests[idx] -= 1;
    }
  }
  return requests;
}

ScenarioMatrixResult LoadScenarioMatrix(const fs::path& path,
                                        size_t default_requests) {
  ScenarioMatrixResult result;
  std::ifstream in(path);
  if (!in) {
    result.error = "Scenario matrix file missing: " + path.string();
    return result;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  std::string contents = buffer.str();
  if (contents.empty() ||
      !ParseScenarioMatrix(contents, default_requests, result.scenarios)) {
    result.error = "Scenario matrix file invalid: " + path.string();
  }
  return result;
}

std::string NowIso8601() {
  std::time_t now = std::time(nullptr);
  std::tm utc = *std::gmtime(&now);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return buffer;
}

double Percentile(const std::vector<double>& sorted, double percentile) {
  if (sorted.empty()) {
    return 0.0;
  }
  auto clamped = std::min(std::max(percentile, 0.0), 1.0);
  double idx = clamped * (static_cast<double>(sorted.size()) - 1.0);
  auto lower = static_cast<size_t>(idx);
  auto upper = std::min(lower + 1, sorted.size() - 1);
  double frac = idx - static_cast<double>(lower);
  return sorted[lower] + (sorted[upper] - sorted[lower]) * frac;
}

void PrintUsage(const char* name) {
  std::cout << "Usage: " << name
            << " [--out path] [--ops N] [--keys N] [--read-ratio R]" << '\n';
}
}  // namespace

int main(int argc, char** argv) {
  Config config;
  std::string out_path = "bench/out/latest.json";
  bool ops_override = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--out" && i + 1 < argc) {
      out_path = argv[++i];
    } else if (arg == "--ops" && i + 1 < argc) {
      config.ops = static_cast<size_t>(std::stoull(argv[++i]));
      ops_override = true;
    } else if (arg == "--keys" && i + 1 < argc) {
      config.key_space = static_cast<size_t>(std::stoull(argv[++i]));
    } else if (arg == "--read-ratio" && i + 1 < argc) {
      config.read_ratio = std::stod(argv[++i]);
    } else if (arg == "--help") {
      PrintUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << '\n';
      PrintUsage(argv[0]);
      return 1;
    }
  }

  if (config.key_space == 0 || config.read_ratio < 0.0 ||
      config.read_ratio > 1.0) {
    std::cerr << "Invalid benchmark configuration." << '\n';
    return 1;
  }

  const fs::path matrix_path = fs::path("bench/scenarios/scenario_matrix.json");
  ScenarioMatrixResult scenario_matrix =
      LoadScenarioMatrix(matrix_path, config.ops);
  if (!scenario_matrix.error.empty()) {
    std::cerr << scenario_matrix.error << '\n';
    return 1;
  }
  const std::vector<ScenarioSpec>& scenario_specs = scenario_matrix.scenarios;
  std::vector<size_t> scenario_requests;
  scenario_requests.reserve(scenario_specs.size());
  if (ops_override) {
    if (config.ops == 0) {
      std::cerr << "Invalid benchmark configuration." << '\n';
      return 1;
    }
    scenario_requests = DistributeScenarioRequests(scenario_specs, config.ops);
  } else {
    size_t total_requests = 0;
    if (!SumScenarioRequests(scenario_specs, total_requests) ||
        total_requests == 0) {
      std::cerr << "Invalid benchmark configuration." << '\n';
      return 1;
    }
    config.ops = total_requests;
    for (const auto& spec : scenario_specs) {
      scenario_requests.push_back(spec.requests);
    }
  }

  ConcurrentStore store(config.stripes, config.key_space * 2);
  std::vector<std::string> keys;
  keys.reserve(config.key_space);
  for (size_t i = 0; i < config.key_space; ++i) {
    keys.push_back("key_" + std::to_string(i));
  }

  for (const auto& key : keys) {
    store.Set(key, "warm", std::nullopt);
  }

  std::mt19937 rng(42);
  std::uniform_real_distribution<double> op_dist(0.0, 1.0);
  std::uniform_int_distribution<size_t> key_dist(0, keys.size() - 1);

  std::vector<double> latencies_ms;
  latencies_ms.reserve(config.ops);
  size_t read_ops = 0;
  size_t write_ops = 0;

  auto bench_start = Clock::now();
  for (size_t i = 0; i < config.ops; ++i) {
    const auto& key = keys[key_dist(rng)];
    bool do_read = op_dist(rng) < config.read_ratio;
    auto op_start = Clock::now();
    if (do_read) {
      (void)store.Get(key);
      ++read_ops;
    } else {
      store.Set(key, "value_" + std::to_string(i), std::nullopt);
      ++write_ops;
    }
    auto op_end = Clock::now();
    latencies_ms.push_back(
        std::chrono::duration<double, std::milli>(op_end - op_start).count());
  }
  auto bench_end = Clock::now();

  std::sort(latencies_ms.begin(), latencies_ms.end());
  double duration_ms =
      std::chrono::duration<double, std::milli>(bench_end - bench_start).count();
  double duration_sec = duration_ms / 1000.0;
  double ops_per_sec = duration_sec > 0.0 ? config.ops / duration_sec : 0.0;

  double p50 = Percentile(latencies_ms, 0.50);
  double p95 = Percentile(latencies_ms, 0.95);
  double p99 = Percentile(latencies_ms, 0.99);

  size_t coalescing_bursts =
      std::max<size_t>(1, std::min<size_t>(20, config.ops / 1000));
  size_t coalescing_concurrency =
      std::max<size_t>(2, std::min<size_t>(8, config.stripes * 2));
  CoalescingBenchmarkResult coalescing_off =
      MeasureCoalescingBehavior(false, coalescing_bursts,
                                coalescing_concurrency);
  CoalescingBenchmarkResult coalescing_on =
      MeasureCoalescingBehavior(true, coalescing_bursts,
                                coalescing_concurrency);

  std::vector<ScenarioResult> scenario_results;
  scenario_results.reserve(scenario_specs.size());
  for (size_t i = 0; i < scenario_specs.size(); ++i) {
    const auto& spec = scenario_specs[i];
    const auto& name = spec.name;
    double coalescing_hit_ratio = 0.0;
    double duplicate_backend_hits = 0.0;
    if (name == "coalescing_off") {
      coalescing_hit_ratio = coalescing_off.coalescing_hit_ratio;
      duplicate_backend_hits = coalescing_off.duplicate_backend_hits;
    } else if (name == "coalescing_on") {
      coalescing_hit_ratio = coalescing_on.coalescing_hit_ratio;
      duplicate_backend_hits = coalescing_on.duplicate_backend_hits;
    }
    scenario_results.push_back(
        {name,
         scenario_requests[i],
         ops_per_sec,
         p50,
         p99,
         0.0,
         coalescing_hit_ratio,
         duplicate_backend_hits,
         "ok"});
  }

  fs::path output = out_path;
  if (!output.parent_path().empty()) {
    fs::create_directories(output.parent_path());
  }
  std::ofstream out(output);
  if (!out) {
    std::cerr << "Failed to open output file: " << output << '\n';
    return 1;
  }

  out << std::fixed << std::setprecision(3);
  out << "{\n";
  out << "  \"timestamp\": \"" << JsonEscapeString(NowIso8601()) << "\",\n";
  out << "  \"ops_total\": " << config.ops << ",\n";
  out << "  \"duration_ms\": " << duration_ms << ",\n";
  out << "  \"ops_per_sec\": " << ops_per_sec << ",\n";
  out << "  \"p50_ms\": " << p50 << ",\n";
  out << "  \"p95_ms\": " << p95 << ",\n";
  out << "  \"p99_ms\": " << p99 << ",\n";
  out << "  \"read_ops\": " << read_ops << ",\n";
  out << "  \"write_ops\": " << write_ops << ",\n";
  out << "  \"read_ratio\": " << config.read_ratio << ",\n";
  out << "  \"key_space\": " << config.key_space << ",\n";
  out << "  \"scenarios\": [\n";
  for (size_t i = 0; i < scenario_results.size(); ++i) {
    const auto& scenario = scenario_results[i];
    out << "    {\n";
    out << "      \"name\": \"" << JsonEscapeString(scenario.name) << "\",\n";
    out << "      \"requests\": " << scenario.requests << ",\n";
    out << "      \"ops_per_sec\": " << scenario.ops_per_sec << ",\n";
    out << "      \"p50_ms\": " << scenario.p50_ms << ",\n";
    out << "      \"p99_ms\": " << scenario.p99_ms << ",\n";
    out << "      \"error_rate\": " << scenario.error_rate << ",\n";
    out << "      \"coalescing_hit_ratio\": " << scenario.coalescing_hit_ratio
        << ",\n";
    out << "      \"duplicate_backend_hits\": "
        << scenario.duplicate_backend_hits << ",\n";
    out << "      \"status\": \"" << JsonEscapeString(scenario.status) << "\"\n";
    out << "    }" << (i + 1 < scenario_results.size() ? "," : "") << "\n";
  }
  out << "  ]\n";
  out << "}\n";

  std::cout << "Benchmark complete. ops/sec=" << std::setprecision(2)
            << std::fixed << ops_per_sec << " p99_ms=" << p99 << '\n';
  return 0;
}
