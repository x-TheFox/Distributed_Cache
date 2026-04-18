#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>

namespace cache::core {
class RequestCoalescer {
 public:
  using Work = std::function<std::string()>;

  std::shared_future<std::string> Execute(const std::string& key, Work work);

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_future<std::string>> inflight_;
};
}  // namespace cache::core
