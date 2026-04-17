#include "core/concurrent_store.hpp"

#include <mutex>

namespace cache::core {
ConcurrentStore::ConcurrentStore(size_t stripes)
    : stripes_(stripes == 0 ? 1 : stripes) {}

void ConcurrentStore::Set(
    std::string key, std::string value,
    std::optional<std::chrono::milliseconds> ttl) {
  auto& stripe = StripeFor(key);
  std::optional<std::chrono::steady_clock::time_point> expires_at =
      ttl.has_value()
          ? std::optional<std::chrono::steady_clock::time_point>(
                std::chrono::steady_clock::now() + *ttl)
          : std::nullopt;
  std::unique_lock lock(stripe.mu);
  stripe.map[std::move(key)] = ValueEntry{std::move(value), expires_at};
}

std::optional<std::string> ConcurrentStore::Get(const std::string& key) {
  auto& stripe = StripeFor(key);
  auto now = std::chrono::steady_clock::now();
  {
    std::shared_lock lock(stripe.mu);
    auto it = stripe.map.find(key);
    if (it == stripe.map.end()) {
      return std::nullopt;
    }
    if (!it->second.IsExpired(now)) {
      return it->second.value;
    }
  }

  std::unique_lock lock(stripe.mu);
  auto it = stripe.map.find(key);
  if (it == stripe.map.end()) {
    return std::nullopt;
  }
  if (it->second.IsExpired(now)) {
    stripe.map.erase(it);
    return std::nullopt;
  }
  return it->second.value;
}

ConcurrentStore::Stripe& ConcurrentStore::StripeFor(const std::string& key) {
  auto idx = std::hash<std::string>{}(key) % stripes_.size();
  return stripes_[idx];
}
}  // namespace cache::core
