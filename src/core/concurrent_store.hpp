#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/value_entry.hpp"

namespace cache::core {
class ConcurrentStore {
 public:
  explicit ConcurrentStore(size_t stripes, size_t max_entries = 0);
  void Set(std::string key, std::string value,
           std::optional<std::chrono::milliseconds> ttl);
  std::optional<std::string> Get(const std::string& key);

 private:
  struct Stripe {
    std::shared_mutex mu;
    std::unordered_map<std::string, ValueEntry> map;
  };

  Stripe& StripeFor(const std::string& key);
  void EvictIfNeeded(Stripe& stripe);
  void TouchEntry(ValueEntry& entry, std::chrono::steady_clock::time_point now);

  std::vector<Stripe> stripes_;
  size_t max_entries_;
  size_t max_entries_per_stripe_;
};
}  // namespace cache::core
