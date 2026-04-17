#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cache::core::eviction {
struct EvictionCandidate {
  std::string key;
  uint32_t lfu_count;
  uint64_t last_touch_ns;
  bool in_protected_segment;
};

std::string SelectVictim(const std::vector<EvictionCandidate>& candidates);
}  // namespace cache::core::eviction
