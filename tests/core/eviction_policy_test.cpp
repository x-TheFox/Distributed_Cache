#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/concurrent_store.hpp"
#include "core/eviction/eviction_policy.hpp"

TEST(EvictionPolicy, KeepsFrequentlyAccessedKeys) {
  cache::core::ConcurrentStore store(1, 3);
  store.Set("cold1", "v1", std::nullopt);
  store.Set("hot", "v-hot", std::nullopt);
  store.Set("cold2", "v2", std::nullopt);

  for (int i = 0; i < 5; ++i) {
    auto value = store.Get("hot");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "v-hot");
  }

  store.Set("cold3", "v3", std::nullopt);
  store.Set("cold4", "v4", std::nullopt);

  auto hot = store.Get("hot");
  ASSERT_TRUE(hot.has_value());
  EXPECT_EQ(*hot, "v-hot");
}

TEST(EvictionPolicy, SelectVictimDeterministicTieBreak) {
  std::vector<cache::core::eviction::EvictionCandidate> candidates = {
      {"b", 1, 100, false},
      {"a", 1, 100, false},
  };

  EXPECT_EQ(cache::core::eviction::SelectVictim(candidates), "a");
}
