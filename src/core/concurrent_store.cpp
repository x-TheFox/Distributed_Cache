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
  if (!entry.in_protected_segment && entry.lfu_count >= 2) {
    entry.in_protected_segment = true;
    protected_entries_.fetch_add(1, std::memory_order_relaxed);
  }
}

std::string ConcurrentStore::SelectVictimKey(
    bool in_protected_segment) {
  std::vector<eviction::EvictionCandidate> candidates;
  for (auto& stripe : stripes_) {
    std::shared_lock lock(stripe.mu);
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

  auto total_entries = total_entries_.load(std::memory_order_relaxed);
  if (total_entries <= max_entries_) {
    return;
  }

  auto now = std::chrono::steady_clock::now();
  for (auto& stripe : stripes_) {
    std::unique_lock lock(stripe.mu);
    for (auto it = stripe.map.begin(); it != stripe.map.end();) {
      if (it->second.IsExpired(now)) {
        if (it->second.in_protected_segment) {
          protected_entries_.fetch_sub(1, std::memory_order_relaxed);
        }
        it = stripe.map.erase(it);
        total_entries_.fetch_sub(1, std::memory_order_relaxed);
      } else {
        ++it;
      }
    }
  }

  total_entries = total_entries_.load(std::memory_order_relaxed);
  if (total_entries < max_entries_) {
    return;
  }

  auto protected_entries = protected_entries_.load(std::memory_order_relaxed);
  while (total_entries > max_entries_ &&
         protected_entries > protected_entries_limit_) {
    auto victim_key = SelectVictimKey(true);
    if (victim_key.empty()) {
      break;
    }
    auto& stripe = StripeFor(victim_key);
    std::unique_lock lock(stripe.mu);
    auto it = stripe.map.find(victim_key);
    if (it == stripe.map.end() || !it->second.in_protected_segment) {
      protected_entries = protected_entries_.load(std::memory_order_relaxed);
      total_entries = total_entries_.load(std::memory_order_relaxed);
      continue;
    }
    it->second.in_protected_segment = false;
    it->second.lfu_count = 1;
    protected_entries_.fetch_sub(1, std::memory_order_relaxed);
    protected_entries = protected_entries_.load(std::memory_order_relaxed);
    total_entries = total_entries_.load(std::memory_order_relaxed);
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
    std::unique_lock lock(stripe.mu);
    auto it = stripe.map.find(victim_key);
    if (it == stripe.map.end()) {
      total_entries = total_entries_.load(std::memory_order_relaxed);
      continue;
    }
    if (it->second.in_protected_segment) {
      protected_entries_.fetch_sub(1, std::memory_order_relaxed);
    }
    stripe.map.erase(it);
    total_entries_.fetch_sub(1, std::memory_order_relaxed);
    total_entries = total_entries_.load(std::memory_order_relaxed);
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
      total_entries_.fetch_add(1, std::memory_order_relaxed);
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
    if (it->second.in_protected_segment) {
      protected_entries_.fetch_sub(1, std::memory_order_relaxed);
    }
    stripe.map.erase(it);
    total_entries_.fetch_sub(1, std::memory_order_relaxed);
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
