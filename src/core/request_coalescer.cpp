#include "core/request_coalescer.hpp"

namespace cache::core {
std::shared_future<std::string> RequestCoalescer::Execute(const std::string& key,
                                                          Work work) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto existing = inflight_.find(key);
  if (existing != inflight_.end()) {
    return existing->second;
  }

  std::promise<std::string> promise;
  auto future = promise.get_future().share();
  inflight_.emplace(key, future);
  lock.unlock();

  try {
    promise.set_value(work());
  } catch (...) {
    promise.set_exception(std::current_exception());
  }

  lock.lock();
  inflight_.erase(key);
  return future;
}
}  // namespace cache::core
