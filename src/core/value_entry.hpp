#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace cache::core {
struct ValueEntry {
  std::string value;
  std::optional<std::chrono::steady_clock::time_point> expires_at;

  bool IsExpired(std::chrono::steady_clock::time_point now) const {
    return expires_at.has_value() && now >= *expires_at;
  }
};
}  // namespace cache::core
