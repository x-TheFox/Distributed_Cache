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
  size_t StripeIndexForKey(const std::string& key) const;
  void EnforceCapacity();
  void TouchEntry(ValueEntry& entry, std::chrono::steady_clock::time_point now);
  std::string SelectVictimKey(bool in_protected_segment) const;

  std::vector<Stripe> stripes_;
  size_t max_entries_;
  size_t protected_entries_limit_;
};
}  // namespace cache::core
