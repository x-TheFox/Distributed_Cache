#include "core/concurrent_store.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <vector>

#include "core/eviction/eviction_policy.hpp"

namespace cache::core {
namespace {
uint64_t ToNs(std::chrono::steady_clock::time_point tp) {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch())
          .count());
}
}  // namespace

ConcurrentStore::ConcurrentStore(size_t stripes, size_t max_entries)
    : stripes_(stripes == 0 ? 1 : stripes),
      max_entries_(max_entries),
      max_entries_per_stripe_(0) {
  if (max_entries_ > 0) {
    auto stripe_count = stripes_.size();
    max_entries_per_stripe_ =
        std::max<size_t>(1, (max_entries_ + stripe_count - 1) / stripe_count);
  }
}

void ConcurrentStore::TouchEntry(ValueEntry& entry,
                                 std::chrono::steady_clock::time_point now) {
  if (entry.lfu_count < std::numeric_limits<uint32_t>::max()) {
    ++entry.lfu_count;
  }
  entry.last_touch_ns = ToNs(now);
  if (entry.lfu_count >= 2) {
    entry.in_protected_segment = true;
  }
}

void ConcurrentStore::EvictIfNeeded(Stripe& stripe) {
  if (max_entries_per_stripe_ == 0) {
    return;
  }

  auto now = std::chrono::steady_clock::now();
  for (auto it = stripe.map.begin(); it != stripe.map.end();) {
    if (it->second.IsExpired(now)) {
      it = stripe.map.erase(it);
    } else {
      ++it;
    }
  }

  while (stripe.map.size() > max_entries_per_stripe_) {
    std::vector<eviction::EvictionCandidate> candidates;
    candidates.reserve(stripe.map.size());
    for (const auto& [key, entry] : stripe.map) {
      candidates.push_back(eviction::EvictionCandidate{
          key, entry.lfu_count, entry.last_touch_ns, entry.in_protected_segment});
    }

    auto victim = eviction::SelectVictim(candidates);
    if (victim.empty()) {
      break;
    }
    stripe.map.erase(victim);
  }
}

void ConcurrentStore::Set(
    std::string key, std::string value,
    std::optional<std::chrono::milliseconds> ttl) {
  auto& stripe = StripeFor(key);
  std::optional<std::chrono::steady_clock::time_point> expires_at =
      ttl.has_value()
          ? std::optional<std::chrono::steady_clock::time_point>(
                std::chrono::steady_clock::now() + *ttl)
          : std::nullopt;
  auto now = std::chrono::steady_clock::now();
  auto now_ns = ToNs(now);

  std::unique_lock lock(stripe.mu);
  auto it = stripe.map.find(key);
  if (it != stripe.map.end()) {
    it->second.value = std::move(value);
    it->second.expires_at = expires_at;
    TouchEntry(it->second, now);
  } else {
    stripe.map.emplace(std::move(key),
                       ValueEntry{std::move(value), expires_at, 1, now_ns, false});
  }
  EvictIfNeeded(stripe);
}

std::optional<std::string> ConcurrentStore::Get(const std::string& key) {
  auto& stripe = StripeFor(key);
  {
    std::shared_lock lock(stripe.mu);
    auto it = stripe.map.find(key);
    if (it == stripe.map.end()) {
      return std::nullopt;
    }
    auto now = std::chrono::steady_clock::now();
    if (!it->second.IsExpired(now)) {
      // Fall through to take exclusive lock to update metadata.
    }
  }

  std::unique_lock lock(stripe.mu);
  auto it = stripe.map.find(key);
  if (it == stripe.map.end()) {
    return std::nullopt;
  }
  auto now = std::chrono::steady_clock::now();
  if (it->second.IsExpired(now)) {
    stripe.map.erase(it);
    return std::nullopt;
  }
  TouchEntry(it->second, now);
  return it->second.value;
}

ConcurrentStore::Stripe& ConcurrentStore::StripeFor(const std::string& key) {
  auto idx = std::hash<std::string>{}(key) % stripes_.size();
  return stripes_[idx];
}
}  // namespace cache::core
