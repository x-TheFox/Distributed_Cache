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
      protected_entries_limit_(0) {
  if (max_entries_ > 0) {
    protected_entries_limit_ = std::max<size_t>(1, max_entries_ / 2);
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

std::string ConcurrentStore::SelectVictimKey(
    bool in_protected_segment) const {
  std::vector<eviction::EvictionCandidate> candidates;
  for (const auto& stripe : stripes_) {
    for (const auto& [key, entry] : stripe.map) {
      if (entry.in_protected_segment != in_protected_segment) {
        continue;
      }
      candidates.push_back(eviction::EvictionCandidate{
          key, entry.lfu_count, entry.last_touch_ns, entry.in_protected_segment});
    }
  }
  return eviction::SelectVictim(candidates);
}

void ConcurrentStore::EnforceCapacity() {
  if (max_entries_ == 0) {
    return;
  }

  std::vector<std::unique_lock<std::shared_mutex>> locks;
  locks.reserve(stripes_.size());
  for (auto& stripe : stripes_) {
    locks.emplace_back(stripe.mu);
  }

  auto now = std::chrono::steady_clock::now();
  size_t total_entries = 0;
  size_t protected_entries = 0;
  for (auto& stripe : stripes_) {
    for (auto it = stripe.map.begin(); it != stripe.map.end();) {
      if (it->second.IsExpired(now)) {
        it = stripe.map.erase(it);
      } else {
        ++total_entries;
        if (it->second.in_protected_segment) {
          ++protected_entries;
        }
        ++it;
      }
    }
  }

  while (protected_entries > protected_entries_limit_) {
    auto victim_key = SelectVictimKey(true);
    if (victim_key.empty()) {
      break;
    }
    auto& stripe = StripeFor(victim_key);
    auto it = stripe.map.find(victim_key);
    if (it == stripe.map.end()) {
      break;
    }
    if (it->second.in_protected_segment) {
      it->second.in_protected_segment = false;
      it->second.lfu_count = 1;
      --protected_entries;
    } else {
      break;
    }
  }

  while (total_entries > max_entries_) {
    auto victim_key = SelectVictimKey(false);
    if (victim_key.empty()) {
      victim_key = SelectVictimKey(true);
    }
    if (victim_key.empty()) {
      break;
    }
    auto& stripe = StripeFor(victim_key);
    auto it = stripe.map.find(victim_key);
    if (it == stripe.map.end()) {
      break;
    }
    if (it->second.in_protected_segment && protected_entries > 0) {
      --protected_entries;
    }
    stripe.map.erase(it);
    --total_entries;
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

  {
    std::unique_lock lock(stripe.mu);
    auto it = stripe.map.find(key);
    if (it != stripe.map.end()) {
      it->second.value = std::move(value);
      it->second.expires_at = expires_at;
      TouchEntry(it->second, now);
    } else {
      stripe.map.emplace(
          std::move(key),
          ValueEntry{std::move(value), expires_at, 1, now_ns, false});
    }
  }
  EnforceCapacity();
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
  auto idx = StripeIndexForKey(key);
  return stripes_[idx];
}

size_t ConcurrentStore::StripeIndexForKey(const std::string& key) const {
  return std::hash<std::string>{}(key) % stripes_.size();
}
}  // namespace cache::core
