#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>

#include "cluster/hash_ring.hpp"
#include "core/concurrent_store.hpp"
#include "metrics/prometheus_endpoint.hpp"
#include "protocol/grpc/cache_service.hpp"
#include "protocol/resp/resp_parser.hpp"
#include "server/node_config.hpp"
#include "server/resp_worker.hpp"

namespace {
constexpr size_t kMaxRespFrameBytes = 1024 * 1024;
constexpr int kListenBacklog = 64;
constexpr int kSelectTimeoutMs = 200;
constexpr int kSocketTimeoutMs = 200;
constexpr int kMaxPort = 65535;

std::atomic<bool>* g_stop_flag = nullptr;

void HandleSignal(int) {
  if (g_stop_flag) {
    g_stop_flag->store(true);
  }
}

bool ParseNumber(std::string_view text, long long* value) {
  if (text.empty()) {
    return false;
  }
  long long result = 0;
  auto begin = text.data();
  auto end = text.data() + text.size();
  auto [ptr, ec] = std::from_chars(begin, end, result);
  if (ec != std::errc{} || ptr != end) {
    return false;
  }
  *value = result;
  return true;
}

bool ParseIntFlag(std::string_view arg, std::string_view prefix, int* out) {
  if (!arg.starts_with(prefix)) {
    return false;
  }
  long long value = 0;
  if (!ParseNumber(arg.substr(prefix.size()), &value)) {
    return false;
  }
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    return false;
  }
  *out = static_cast<int>(value);
  return true;
}

bool ParseDurationFlag(std::string_view arg, std::string_view prefix,
                       std::chrono::milliseconds* out) {
  if (!arg.starts_with(prefix)) {
    return false;
  }
  long long value = 0;
  if (!ParseNumber(arg.substr(prefix.size()), &value)) {
    return false;
  }
  if (value < 0) {
    return false;
  }
  *out = std::chrono::milliseconds(value);
  return true;
}

bool ParseStringFlag(std::string_view arg, std::string_view prefix,
                     std::string* out) {
  if (!arg.starts_with(prefix)) {
    return false;
  }
  *out = std::string(arg.substr(prefix.size()));
  return true;
}

bool TryAcquireRespSlot(std::atomic<int>& active, int max_clients) {
  if (max_clients <= 0) {
    return true;
  }
  int current = active.load(std::memory_order_relaxed);
  while (true) {
    if (current >= max_clients) {
      return false;
    }
    if (active.compare_exchange_weak(current, current + 1,
                                     std::memory_order_acq_rel,
                                     std::memory_order_relaxed)) {
      return true;
    }
  }
}

struct RuntimeOptions {
  cache::server::NodeConfig config{cache::server::NodeConfig::Default()};
  bool dry_run{false};
  std::optional<std::chrono::milliseconds> run_for;
};

bool PortInRange(int port) {
  return port >= 0 && port <= kMaxPort;
}

std::optional<RuntimeOptions> ParseArgs(int argc, char** argv) {
  RuntimeOptions options;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    if (arg == "--dry-run") {
      options.dry_run = true;
      continue;
    }
    std::chrono::milliseconds run_for{};
    if (ParseDurationFlag(arg, "--run-for-ms=", &run_for)) {
      options.run_for = run_for;
      continue;
    }
    int value = 0;
    if (ParseIntFlag(arg, "--shard-count=", &value)) {
      options.config.shard_count = value;
      continue;
    }
    if (ParseIntFlag(arg, "--virtual-nodes=", &value)) {
      options.config.virtual_nodes = value;
      continue;
    }
    if (ParseIntFlag(arg, "--max-resp-clients=", &value)) {
      options.config.max_resp_clients = value;
      continue;
    }
    if (ParseIntFlag(arg, "--listen-port=", &value) ||
        ParseIntFlag(arg, "--resp-port=", &value)) {
      options.config.listen_port = value;
      continue;
    }
    if (ParseIntFlag(arg, "--grpc-port=", &value)) {
      options.config.grpc_port = value;
      continue;
    }
    if (ParseIntFlag(arg, "--metrics-port=", &value)) {
      options.config.metrics_port = value;
      continue;
    }
    std::chrono::milliseconds resp_idle_timeout{};
    if (ParseDurationFlag(arg, "--resp-idle-timeout-ms=", &resp_idle_timeout)) {
      if (resp_idle_timeout.count() >
          std::numeric_limits<int>::max()) {
        std::cerr << "resp-idle-timeout-ms out of range\n";
        return std::nullopt;
      }
      options.config.resp_idle_timeout_ms =
          static_cast<int>(resp_idle_timeout.count());
      continue;
    }
    std::string text_value;
    if (ParseStringFlag(arg, "--node-id=", &text_value)) {
      options.config.node_id = std::move(text_value);
      continue;
    }
    std::cerr << "Unknown argument: " << arg << "\n";
    return std::nullopt;
  }

  if (options.config.shard_count <= 0 || options.config.virtual_nodes <= 0) {
    std::cerr << "Invalid shard or virtual node count\n";
    return std::nullopt;
  }
  if (!PortInRange(options.config.listen_port) ||
      !PortInRange(options.config.grpc_port) ||
      !PortInRange(options.config.metrics_port)) {
    std::cerr << "Ports must be between 0 and 65535\n";
    return std::nullopt;
  }
  if (options.config.max_resp_clients <= 0) {
    std::cerr << "max-resp-clients must be positive\n";
    return std::nullopt;
  }
  if (options.config.resp_idle_timeout_ms <= 0) {
    std::cerr << "resp-idle-timeout-ms must be positive\n";
    return std::nullopt;
  }
  if (options.config.node_id.empty()) {
    std::cerr << "node-id must not be empty\n";
    return std::nullopt;
  }
  return options;
}

int CreateListeningSocket(int port, int* bound_port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  int yes = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
    ::close(fd);
    return -1;
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return -1;
  }
  if (::listen(fd, kListenBacklog) != 0) {
    ::close(fd);
    return -1;
  }
  if (bound_port) {
    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
      *bound_port = ntohs(addr.sin_port);
    }
  }
  return fd;
}

void ApplySocketTimeout(int fd) {
  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = kSocketTimeoutMs * 1000;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

bool SendAll(int fd, std::string_view payload) {
  while (!payload.empty()) {
    auto sent = ::send(fd, payload.data(), payload.size(), 0);
    if (sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    payload.remove_prefix(static_cast<size_t>(sent));
  }
  return true;
}

void HandleRespClient(int client_fd, cache::core::ConcurrentStore& store,
                       cache::metrics::PrometheusEndpoint& metrics,
                       std::atomic<bool>& stop,
                       std::chrono::milliseconds idle_timeout) {
  ApplySocketTimeout(client_fd);
  std::string buffer;
  buffer.reserve(1024);
  auto last_activity = std::chrono::steady_clock::now();
  while (!stop.load()) {
    char chunk[4096];
    auto read = ::recv(client_fd, chunk, sizeof(chunk), 0);
    if (read == 0) {
      return;
    }
    if (read < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_activity > idle_timeout) {
          return;
        }
        continue;
      }
      return;
    }
    last_activity = std::chrono::steady_clock::now();
    buffer.append(chunk, static_cast<size_t>(read));
    while (!stop.load()) {
      auto parsed = cache::protocol::resp::ParseRESP(buffer);
      if (parsed.command.name.empty() || parsed.bytes_consumed == 0) {
        break;
      }
      auto response =
          cache::protocol::resp::ExecuteCommand(parsed.command, store);
      metrics.IncrementCounter("cache_resp_requests_total", 1.0,
                               "Total RESP requests");
      if (!SendAll(client_fd, response)) {
        return;
      }
      buffer.erase(0, parsed.bytes_consumed);
      if (buffer.empty()) {
        break;
      }
    }
    if (buffer.size() > kMaxRespFrameBytes) {
      SendAll(client_fd, "-ERR invalid command\r\n");
      return;
    }
  }
}

void RequestStop(std::atomic<bool>& stop, std::condition_variable& stop_cv) {
  stop.store(true);
  stop_cv.notify_all();
}

void RunRespServer(int port, cache::core::ConcurrentStore& store,
                    cache::metrics::PrometheusEndpoint& metrics,
                    int max_resp_clients,
                    std::chrono::milliseconds resp_idle_timeout,
                    std::atomic<int>& active_resp_clients,
                    std::atomic<bool>& stop, std::atomic<bool>& failed,
                    std::condition_variable& stop_cv) {
  int bound_port = 0;
  int server_fd = CreateListeningSocket(port, &bound_port);
  if (server_fd < 0) {
    std::cerr << "Failed to bind RESP port " << port << "\n";
    failed.store(true);
    RequestStop(stop, stop_cv);
    return;
  }
  while (!stop.load()) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = kSelectTimeoutMs * 1000;
    int result = ::select(server_fd + 1, &readfds, nullptr, nullptr, &timeout);
    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (result == 0) {
      continue;
    }
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd =
        ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr),
                 &client_len);
    if (client_fd < 0) {
      continue;
    }
    if (!TryAcquireRespSlot(active_resp_clients, max_resp_clients)) {
      ApplySocketTimeout(client_fd);
      SendAll(client_fd, "-ERR max clients reached\r\n");
      ::close(client_fd);
      continue;
    }
    if (!cache::server::StartRespWorker(
            client_fd, active_resp_clients,
            [&](int fd) {
              HandleRespClient(fd, store, metrics, stop, resp_idle_timeout);
            })) {
      continue;
    }
  }
  ::close(server_fd);
}

void HandleMetricsClient(int client_fd,
                         cache::metrics::PrometheusEndpoint& metrics) {
  ApplySocketTimeout(client_fd);
  char buffer[1024];
  ::recv(client_fd, buffer, sizeof(buffer), 0);
  auto body = metrics.RenderPrometheusMetrics();
  std::string response =
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain; version=0.0.4\r\n";
  response += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
  response += body;
  SendAll(client_fd, response);
}

void RunMetricsServer(int port, cache::metrics::PrometheusEndpoint& metrics,
                      std::atomic<bool>& stop, std::atomic<bool>& failed,
                      std::condition_variable& stop_cv) {
  int bound_port = 0;
  int server_fd = CreateListeningSocket(port, &bound_port);
  if (server_fd < 0) {
    std::cerr << "Failed to bind metrics port " << port << "\n";
    failed.store(true);
    RequestStop(stop, stop_cv);
    return;
  }
  while (!stop.load()) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = kSelectTimeoutMs * 1000;
    int result = ::select(server_fd + 1, &readfds, nullptr, nullptr, &timeout);
    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (result == 0) {
      continue;
    }
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd =
        ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr),
                 &client_len);
    if (client_fd < 0) {
      continue;
    }
    metrics.IncrementCounter("cache_metrics_requests_total", 1.0,
                             "Total metrics scrapes");
    HandleMetricsClient(client_fd, metrics);
    ::close(client_fd);
  }
  ::close(server_fd);
}

}  // namespace

int main(int argc, char** argv) {
  auto options = ParseArgs(argc, argv);
  if (!options.has_value()) {
    return 1;
  }
  cache::core::ConcurrentStore store(
      static_cast<size_t>(options->config.shard_count));
  cache::cluster::HashRing ring;
  ring.AddNode(options->config.node_id, options->config.virtual_nodes);
  if (cache::cluster::RouteKeyToNode(ring, "bootstrap").empty()) {
    std::cerr << "Failed to initialize hash ring\n";
    return 1;
  }
  cache::metrics::PrometheusEndpoint metrics;
  metrics.SetGauge("cache_bootstrap_ok", 1.0,
                   "Cache node bootstrap status");

  if (options->dry_run) {
    return 0;
  }

  std::atomic<bool> stop{false};
  std::atomic<bool> listener_failed{false};
  std::atomic<int> active_resp_clients{0};
  std::mutex stop_mutex;
  std::condition_variable stop_cv;
  g_stop_flag = &stop;
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  cache::protocol::grpc::CacheServiceImpl service(store);
  grpc::ServerBuilder builder;
  int selected_port = 0;
  builder.AddListeningPort(
      "0.0.0.0:" + std::to_string(options->config.grpc_port),
      grpc::InsecureServerCredentials(), &selected_port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  if (!server) {
    std::cerr << "Failed to start gRPC server\n";
    return 1;
  }

  std::thread resp_thread([&]() {
    RunRespServer(
        options->config.listen_port, store, metrics,
        options->config.max_resp_clients,
        std::chrono::milliseconds(options->config.resp_idle_timeout_ms),
        active_resp_clients, stop, listener_failed, stop_cv);
  });
  std::thread metrics_thread([&]() {
    RunMetricsServer(options->config.metrics_port, metrics, stop,
                     listener_failed, stop_cv);
  });
  std::thread grpc_thread([&]() { server->Wait(); });

  std::optional<std::thread> timer_thread;
  if (options->run_for.has_value()) {
    auto run_for = *options->run_for;
    timer_thread.emplace([run_for, &stop, &stop_mutex, &stop_cv]() {
      std::unique_lock<std::mutex> lock(stop_mutex);
      if (stop_cv.wait_for(lock, run_for, [&stop]() { return stop.load(); })) {
        return;
      }
      RequestStop(stop, stop_cv);
    });
  }

  while (!stop.load()) {
    std::unique_lock<std::mutex> lock(stop_mutex);
    stop_cv.wait_for(lock, std::chrono::milliseconds(50),
                     [&stop]() { return stop.load(); });
  }
  stop_cv.notify_all();

  server->Shutdown();
  if (grpc_thread.joinable()) {
    grpc_thread.join();
  }
  if (resp_thread.joinable()) {
    resp_thread.join();
  }
  if (metrics_thread.joinable()) {
    metrics_thread.join();
  }
  if (timer_thread && timer_thread->joinable()) {
    timer_thread->join();
  }
  return listener_failed.load() ? 1 : 0;
}
