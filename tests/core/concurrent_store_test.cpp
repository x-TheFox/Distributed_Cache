#include <atomic>
#include <barrier>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "core/concurrent_store.hpp"

TEST(ConcurrentStore, SetGetRoundTrip) {
  cache::core::ConcurrentStore store(16);
  store.Set("k1", "v1", std::nullopt);
  EXPECT_EQ(store.Get("k1").value(), "v1");
}

TEST(ConcurrentStore, ExpiredEntryIsNotReturned) {
  cache::core::ConcurrentStore store(16);
  store.Set("k2", "v2", std::chrono::milliseconds(0));
  EXPECT_FALSE(store.Get("k2").has_value());
}

TEST(ConcurrentStore, OverwriteClearsExpiration) {
  cache::core::ConcurrentStore store(16);
  store.Set("k3", "v3", std::chrono::milliseconds(0));
  EXPECT_FALSE(store.Get("k3").has_value());

  store.Set("k3", "v4", std::nullopt);
  auto value = store.Get("k3");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "v4");
}

TEST(ConcurrentStore, ConcurrentSetGetIsConsistent) {
  cache::core::ConcurrentStore store(32);
  constexpr int kThreads = 8;
  constexpr int kIterations = 100;
  std::atomic<int> errors{0};
  std::barrier start_gate(kThreads);
  std::vector<std::thread> workers;
  workers.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&store, &errors, &start_gate, t]() {
      start_gate.arrive_and_wait();
      for (int i = 0; i < kIterations; ++i) {
        auto key = "k" + std::to_string(t) + "_" + std::to_string(i);
        auto value = "v" + std::to_string(i);
        store.Set(key, value, std::nullopt);
        auto found = store.Get(key);
        if (!found.has_value() || *found != value) {
          errors.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  EXPECT_EQ(errors.load(std::memory_order_relaxed), 0);
}
