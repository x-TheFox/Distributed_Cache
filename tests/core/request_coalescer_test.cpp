#include <atomic>
#include <barrier>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "core/request_coalescer.hpp"

namespace cache::core {
TEST(RequestCoalescer, DeduplicatesHotMissInflightRequests) {
  RequestCoalescer coalescer;
  std::atomic<int> executions{0};
  constexpr int kThreads = 6;
  std::barrier start_gate(kThreads);
  std::vector<std::thread> workers;
  std::vector<std::string> results(kThreads);

  auto work = [&executions]() {
    executions.fetch_add(1, std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    return std::string("payload");
  };

  for (int i = 0; i < kThreads; ++i) {
    workers.emplace_back([&coalescer, &start_gate, &results, &work, i]() {
      start_gate.arrive_and_wait();
      auto future = coalescer.Execute("hot-key", work);
      results[i] = future.get();
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  EXPECT_EQ(executions.load(std::memory_order_relaxed), 1);
  for (const auto& value : results) {
    EXPECT_EQ(value, "payload");
  }
}
}  // namespace cache::core
