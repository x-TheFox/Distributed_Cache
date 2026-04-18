#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cache::control {
struct NodeHealth {
  std::string node_id;
  bool alive;
  uint64_t last_heartbeat_ms;
};

class HeartbeatManager {
 public:
  void RegisterNode(const std::string& node_id, uint64_t heartbeat_ms = 0);
  void RecordHeartbeat(const std::string& node_id, uint64_t heartbeat_ms);
  void OnHeartbeatTimeout(const std::string& node_id, uint64_t timeout_ms);
  void IngestGossip(const std::vector<NodeHealth>& observations);

  std::optional<NodeHealth> GetNodeHealth(const std::string& node_id) const;
  std::vector<NodeHealth> Snapshot() const;
  bool IsAlive(const std::string& node_id) const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, NodeHealth> nodes_;
  std::unordered_map<std::string, uint64_t> last_observation_ms_;
};
}  // namespace cache::control
