#include <chrono>
#include <csignal>
#include <cstdlib>
#include <errno.h>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "server/node_config.hpp"

namespace {
std::filesystem::path CacheServerPath() {
  auto candidate = std::filesystem::current_path() / "cache_server";
  if (std::filesystem::exists(candidate)) {
    return candidate;
  }
  return candidate;
}

class ScopedListener {
 public:
  ScopedListener() = default;
  ~ScopedListener() {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }
  ScopedListener(const ScopedListener&) = delete;
  ScopedListener& operator=(const ScopedListener&) = delete;
  ScopedListener(ScopedListener&& other) noexcept
      : fd_(other.fd_), port_(other.port_) {
    other.fd_ = -1;
  }
  ScopedListener& operator=(ScopedListener&& other) noexcept {
    if (this != &other) {
      if (fd_ >= 0) {
        ::close(fd_);
      }
      fd_ = other.fd_;
      port_ = other.port_;
      other.fd_ = -1;
    }
    return *this;
  }

  static std::optional<ScopedListener> ListenOnEphemeralPort() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      return std::nullopt;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
      ::close(fd);
      return std::nullopt;
    }
    if (::listen(fd, 1) != 0) {
      ::close(fd);
      return std::nullopt;
    }
    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
      ::close(fd);
      return std::nullopt;
    }
    ScopedListener listener;
    listener.fd_ = fd;
    listener.port_ = ntohs(addr.sin_port);
    return listener;
  }

  int port() const { return port_; }

 private:
  int fd_{-1};
  int port_{0};
};

struct ProcessHandle {
  pid_t pid{-1};
};

std::optional<ProcessHandle> StartCacheServer(
    const std::filesystem::path& path,
    const std::vector<std::string>& args) {
  pid_t pid = ::fork();
  if (pid < 0) {
    return std::nullopt;
  }
  if (pid == 0) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char*>(path.c_str()));
    for (const auto& arg : args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    ::execv(path.c_str(), argv.data());
    _exit(127);
  }
  return ProcessHandle{pid};
}

std::optional<int> WaitForExit(pid_t pid,
                               std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  int status = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    pid_t result = ::waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
      }
      if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
      }
      return std::nullopt;
    }
    if (result < 0) {
      return std::nullopt;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return std::nullopt;
}

void TerminateProcess(pid_t pid) {
  if (pid <= 0) {
    return;
  }
  ::kill(pid, SIGTERM);
  if (WaitForExit(pid, std::chrono::milliseconds(500)).has_value()) {
    return;
  }
  ::kill(pid, SIGKILL);
  WaitForExit(pid, std::chrono::milliseconds(500));
}

int RunCacheServerBlocking(const std::filesystem::path& path,
                           const std::vector<std::string>& args) {
  auto handle = StartCacheServer(path, args);
  if (!handle.has_value()) {
    return -1;
  }
  auto exit_code = WaitForExit(handle->pid, std::chrono::seconds(5));
  if (!exit_code.has_value()) {
    TerminateProcess(handle->pid);
    return -1;
  }
  return *exit_code;
}

bool WaitForPortOpen(int port, std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    int result = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    if (result == 0) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

std::optional<int> ConnectRespClient(int port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return std::nullopt;
  }
  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = 200 * 1000;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return std::nullopt;
  }
  return fd;
}

std::string ReceiveUntil(int fd, std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    char buffer[4096];
    ssize_t read = ::recv(fd, buffer, sizeof(buffer), 0);
    if (read > 0) {
      return std::string(buffer, buffer + read);
    }
    if (read == 0) {
      return {};
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    return {};
  }
  return {};
}

bool SendAll(int fd, std::string_view payload) {
  while (!payload.empty()) {
    auto sent = ::send(fd, payload.data(), payload.size(), 0);
    if (sent <= 0) {
      return false;
    }
    payload.remove_prefix(static_cast<size_t>(sent));
  }
  return true;
}

std::string ReceiveAll(int fd) {
  std::string response;
  char buffer[4096];
  ssize_t read = ::recv(fd, buffer, sizeof(buffer), 0);
  if (read > 0) {
    response.assign(buffer, buffer + read);
  }
  return response;
}

std::optional<std::string> QueryResp(int port, std::string_view request) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return std::nullopt;
  }
  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = 200 * 1000;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return std::nullopt;
  }
  if (!SendAll(fd, request)) {
    ::close(fd);
    return std::nullopt;
  }
  auto response = ReceiveAll(fd);
  ::close(fd);
  return response;
}

std::optional<std::string> QueryMetrics(int port) {
  return QueryResp(
      port,
      "GET /metrics HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
}
}  // namespace

TEST(Bootstrap, DefaultConfigIsValid) {
  auto cfg = cache::server::NodeConfig::Default();
  EXPECT_GT(cfg.shard_count, 0);
  EXPECT_GT(cfg.listen_port, 0);
  EXPECT_GT(cfg.grpc_port, 0);
  EXPECT_GT(cfg.metrics_port, 0);
  EXPECT_FALSE(cfg.node_id.empty());
  EXPECT_GT(cfg.virtual_nodes, 0);
  EXPECT_GT(cfg.max_resp_clients, 0);
  EXPECT_GT(cfg.resp_idle_timeout_ms, 0);
}

TEST(Bootstrap, CacheServerDryRunSucceeds) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto result = RunCacheServerBlocking(server_path, {"--dry-run"});
  EXPECT_EQ(result, 0);
}

TEST(Bootstrap, CacheServerBoundedRunSucceeds) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto result = RunCacheServerBlocking(
      server_path,
      {"--run-for-ms=100", "--resp-port=0", "--grpc-port=0",
       "--metrics-port=0"});
  EXPECT_EQ(result, 0);
}

TEST(Bootstrap, CacheServerRejectsOutOfRangePorts) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto result = RunCacheServerBlocking(
      server_path,
      {"--run-for-ms=100", "--resp-port=70000", "--grpc-port=0",
       "--metrics-port=0"});
  EXPECT_NE(result, 0);
}

TEST(Bootstrap, CacheServerFailsWhenRespPortBusy) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto listener = ScopedListener::ListenOnEphemeralPort();
  ASSERT_TRUE(listener.has_value());
  auto result = RunCacheServerBlocking(
      server_path,
      {"--run-for-ms=200", "--resp-port=" + std::to_string(listener->port()),
       "--grpc-port=0", "--metrics-port=0"});
  EXPECT_NE(result, 0);
}

TEST(Bootstrap, CacheServerFailsWhenMetricsPortBusy) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto listener = ScopedListener::ListenOnEphemeralPort();
  ASSERT_TRUE(listener.has_value());
  auto result = RunCacheServerBlocking(
      server_path,
      {"--run-for-ms=200", "--metrics-port=" + std::to_string(listener->port()),
       "--grpc-port=0", "--resp-port=0"});
  EXPECT_NE(result, 0);
}

TEST(Bootstrap, CacheServerStartsListenersAndServesTraffic) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto resp_port = ScopedListener::ListenOnEphemeralPort();
  auto metrics_port = ScopedListener::ListenOnEphemeralPort();
  ASSERT_TRUE(resp_port.has_value());
  ASSERT_TRUE(metrics_port.has_value());
  int resp = resp_port->port();
  int metrics = metrics_port->port();
  resp_port.reset();
  metrics_port.reset();

  auto handle = StartCacheServer(
      server_path,
      {"--run-for-ms=400", "--resp-port=" + std::to_string(resp),
       "--metrics-port=" + std::to_string(metrics), "--grpc-port=0"});
  ASSERT_TRUE(handle.has_value());
  ASSERT_TRUE(WaitForPortOpen(resp, std::chrono::milliseconds(500)));
  ASSERT_TRUE(WaitForPortOpen(metrics, std::chrono::milliseconds(500)));

  auto resp_response =
      QueryResp(resp, "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");
  ASSERT_TRUE(resp_response.has_value());
  EXPECT_EQ(*resp_response, "$-1\r\n");

  auto metrics_response = QueryMetrics(metrics);
  ASSERT_TRUE(metrics_response.has_value());
  EXPECT_NE(metrics_response->find("HTTP/1.1 200 OK"), std::string::npos);

  auto exit_code = WaitForExit(handle->pid, std::chrono::seconds(2));
  if (!exit_code.has_value()) {
    TerminateProcess(handle->pid);
    FAIL() << "cache_server did not exit";
  }
  EXPECT_EQ(*exit_code, 0);
}

TEST(Bootstrap, RespRejectsWhenClientLimitReached) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto resp_port = ScopedListener::ListenOnEphemeralPort();
  ASSERT_TRUE(resp_port.has_value());
  int resp = resp_port->port();
  resp_port.reset();

  auto handle = StartCacheServer(
      server_path,
      {"--run-for-ms=1000", "--resp-port=" + std::to_string(resp),
       "--grpc-port=0", "--metrics-port=0", "--max-resp-clients=1"});
  ASSERT_TRUE(handle.has_value());
  ASSERT_TRUE(WaitForPortOpen(resp, std::chrono::milliseconds(500)));

  auto first_fd = ConnectRespClient(resp);
  ASSERT_TRUE(first_fd.has_value());
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto second_fd = ConnectRespClient(resp);
  ASSERT_TRUE(second_fd.has_value());
  auto response = ReceiveUntil(*second_fd, std::chrono::milliseconds(500));
  EXPECT_NE(response.find("-ERR max clients reached"), std::string::npos);

  ::close(*second_fd);
  ::close(*first_fd);

  auto exit_code = WaitForExit(handle->pid, std::chrono::seconds(2));
  if (!exit_code.has_value()) {
    TerminateProcess(handle->pid);
    FAIL() << "cache_server did not exit";
  }
  EXPECT_EQ(*exit_code, 0);
}

TEST(Bootstrap, CacheServerStopsPromptlyOnSignal) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto resp_port = ScopedListener::ListenOnEphemeralPort();
  ASSERT_TRUE(resp_port.has_value());
  int resp = resp_port->port();
  resp_port.reset();

  auto handle = StartCacheServer(
      server_path,
      {"--run-for-ms=5000", "--resp-port=" + std::to_string(resp),
       "--metrics-port=0", "--grpc-port=0"});
  ASSERT_TRUE(handle.has_value());
  ASSERT_TRUE(WaitForPortOpen(resp, std::chrono::milliseconds(500)));

  ::kill(handle->pid, SIGTERM);
  auto exit_code = WaitForExit(handle->pid, std::chrono::milliseconds(800));
  if (!exit_code.has_value()) {
    TerminateProcess(handle->pid);
    FAIL() << "cache_server did not exit promptly after SIGTERM";
  }
  EXPECT_EQ(*exit_code, 0);
}
