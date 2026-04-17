#include <chrono>
#include <optional>
#include <thread>

#include <gtest/gtest.h>

#include "core/concurrent_store.hpp"

TEST(ConcurrentStore, SetGetRoundTrip) {
  cache::core::ConcurrentStore store(16);
  store.Set("k1", "v1", std::nullopt);
  EXPECT_EQ(store.Get("k1").value(), "v1");
}

TEST(ConcurrentStore, ExpiredEntryIsNotReturned) {
  cache::core::ConcurrentStore store(16);
  store.Set("k2", "v2", std::chrono::milliseconds(10));
  std::this_thread::sleep_for(std::chrono::milliseconds(15));
  EXPECT_FALSE(store.Get("k2").has_value());
}
