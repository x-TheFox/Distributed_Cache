#pragma once

#include <chrono>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/value_entry.hpp"

namespace cache::core {
class ConcurrentStore {
 public:
  explicit ConcurrentStore(size_t stripes);
  void Set(std::string key, std::string value,
           std::optional<std::chrono::milliseconds> ttl);
  std::optional<std::string> Get(const std::string& key);

 private:
  struct Stripe {
    std::shared_mutex mu;
    std::unordered_map<std::string, ValueEntry> map;
  };

  Stripe& StripeFor(const std::string& key);

  std::vector<Stripe> stripes_;
};
}  // namespace cache::core
