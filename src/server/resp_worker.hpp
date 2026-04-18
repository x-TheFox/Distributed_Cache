#pragma once

#include <atomic>
#include <functional>
#include <system_error>
#include <thread>
#include <utility>

#include <unistd.h>

namespace cache::server {

using RespClientHandler = std::function<void(int)>;
using RespThreadLauncher = std::function<void(std::function<void()>)>;

inline void ReleaseRespSlot(std::atomic<int>& active_resp_clients) {
  active_resp_clients.fetch_sub(1, std::memory_order_acq_rel);
}

inline void DefaultRespThreadLauncher(std::function<void()> task) {
  std::thread(std::move(task)).detach();
}

inline bool StartRespWorker(int client_fd,
                            std::atomic<int>& active_resp_clients,
                            RespClientHandler handler,
                            RespThreadLauncher launcher = {}) {
  auto worker = [client_fd, &active_resp_clients,
                 handler = std::move(handler)]() mutable {
    handler(client_fd);
    ::close(client_fd);
    ReleaseRespSlot(active_resp_clients);
  };

  auto cleanup = [client_fd, &active_resp_clients]() {
    ::close(client_fd);
    ReleaseRespSlot(active_resp_clients);
  };

  if (!launcher) {
    launcher = DefaultRespThreadLauncher;
  }

  try {
    launcher(std::move(worker));
  } catch (const std::system_error&) {
    cleanup();
    return false;
  }

  return true;
}

}  // namespace cache::server
