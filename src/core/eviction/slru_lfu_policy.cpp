#include "core/eviction/eviction_policy.hpp"

#include <tuple>

namespace cache::core::eviction {
namespace {
std::tuple<int, uint32_t, uint64_t, const std::string&> Score(
    const EvictionCandidate& candidate) {
  return std::tuple<int, uint32_t, uint64_t, const std::string&>(
      candidate.in_protected_segment ? 1 : 0, candidate.lfu_count,
      candidate.last_touch_ns, candidate.key);
}
}  // namespace

std::string SelectVictim(const std::vector<EvictionCandidate>& candidates) {
  if (candidates.empty()) {
    return {};
  }

  const EvictionCandidate* victim = &candidates.front();
  for (const auto& candidate : candidates) {
    if (Score(candidate) < Score(*victim)) {
      victim = &candidate;
    }
  }
  return victim->key;
}
}  // namespace cache::core::eviction
