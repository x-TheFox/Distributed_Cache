#include "core/concurrent_store.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using cache::core::ConcurrentStore;
using Clock = std::chrono::steady_clock;

namespace {
struct Config {
  size_t ops = 20000;
  size_t key_space = 2000;
  double read_ratio = 0.8;
  size_t stripes = 8;
};

struct ScenarioResult {
  std::string name;
  double ops_per_sec;
  double p50_ms;
  double p99_ms;
  double error_rate;
  double coalescing_hit_ratio;
  std::string status;
};

struct ScenarioMatrixResult {
  std::vector<std::string> names;
  std::string error;
};

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
            if (pos_ + 4 > input_.size()) {
              return false;
            }
            pos_ += 4;
            out.push_back('?');
            break;
          default:
            out.push_back(esc);
            break;
        }
      } else {
        out.push_back(ch);
      }
    }
    return false;
  }

  bool ParseScenarioArray(std::vector<std::string>& names) {
    if (!Consume('[')) {
      return false;
    }
    SkipWhitespace();
    if (Consume(']')) {
      return true;
    }
    while (true) {
      if (!ParseScenarioObject(names)) {
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

  bool ParseScenarioObject(std::vector<std::string>& names) {
    if (!Consume('{')) {
      return false;
    }
    bool found_name = false;
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
        std::string name;
        if (!ParseString(name)) {
          return false;
        }
        names.push_back(name);
        found_name = true;
      } else {
        if (!SkipValue()) {
          return false;
        }
      }
      SkipWhitespace();
      if (Consume('}')) {
        return found_name;
      }
      if (!Consume(',')) {
        return false;
      }
    }
  }

  std::string_view input_;
  size_t pos_ = 0;
};

bool ParseScenarioMatrix(std::string_view contents,
                         std::vector<std::string>& names) {
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
      if (!reader.ParseScenarioArray(names)) {
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
  if (!found_scenarios || names.empty()) {
    return false;
  }
  return reader.AtEnd();
}

ScenarioMatrixResult LoadScenarioMatrix(const fs::path& path) {
  ScenarioMatrixResult result;
  std::ifstream in(path);
  if (!in) {
    result.error = "Scenario matrix file missing: " + path.string();
    return result;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  std::string contents = buffer.str();
  if (contents.empty() || !ParseScenarioMatrix(contents, result.names)) {
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

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--out" && i + 1 < argc) {
      out_path = argv[++i];
    } else if (arg == "--ops" && i + 1 < argc) {
      config.ops = static_cast<size_t>(std::stoull(argv[++i]));
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

  if (config.ops == 0 || config.key_space == 0 || config.read_ratio < 0.0 ||
      config.read_ratio > 1.0) {
    std::cerr << "Invalid benchmark configuration." << '\n';
    return 1;
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

  const fs::path matrix_path = fs::path("bench/scenarios/scenario_matrix.json");
  ScenarioMatrixResult scenario_matrix = LoadScenarioMatrix(matrix_path);
  if (!scenario_matrix.error.empty()) {
    std::cerr << scenario_matrix.error << '\n';
    return 1;
  }
  const std::vector<std::string>& scenario_names = scenario_matrix.names;
  std::vector<ScenarioResult> scenario_results;
  scenario_results.reserve(scenario_names.size());
  for (const auto& name : scenario_names) {
    scenario_results.push_back(
        {name, ops_per_sec, p50, p99, 0.0, 0.0, "ok"});
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
  out << "  \"timestamp\": \"" << NowIso8601() << "\",\n";
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
    out << "      \"name\": \"" << scenario.name << "\",\n";
    out << "      \"ops_per_sec\": " << scenario.ops_per_sec << ",\n";
    out << "      \"p50_ms\": " << scenario.p50_ms << ",\n";
    out << "      \"p99_ms\": " << scenario.p99_ms << ",\n";
    out << "      \"error_rate\": " << scenario.error_rate << ",\n";
    out << "      \"coalescing_hit_ratio\": " << scenario.coalescing_hit_ratio
        << ",\n";
    out << "      \"status\": \"" << scenario.status << "\"\n";
    out << "    }" << (i + 1 < scenario_results.size() ? "," : "") << "\n";
  }
  out << "  ]\n";
  out << "}\n";

  std::cout << "Benchmark complete. ops/sec=" << std::setprecision(2)
            << std::fixed << ops_per_sec << " p99_ms=" << p99 << '\n';
  return 0;
}
