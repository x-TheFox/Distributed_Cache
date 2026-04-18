#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace cache::core {
struct ValueEntry {
  std::string value;
  std::optional<std::chrono::steady_clock::time_point> expires_at;
  uint32_t lfu_count = 0;
  uint64_t last_touch_ns = 0;
  bool in_protected_segment = false;

  bool IsExpired(std::chrono::steady_clock::time_point now) const {
    return expires_at.has_value() && now >= *expires_at;
  }
};
}  // namespace cache::core
