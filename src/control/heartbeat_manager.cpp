#include "control/heartbeat_manager.hpp"

namespace cache::control {
void HeartbeatManager::RegisterNode(const std::string& node_id,
                                    uint64_t heartbeat_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& entry = nodes_[node_id];
  entry.node_id = node_id;
  entry.alive = true;
  entry.last_heartbeat_ms = heartbeat_ms;
}

void HeartbeatManager::RecordHeartbeat(const std::string& node_id,
                                       uint64_t heartbeat_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& entry = nodes_[node_id];
  entry.node_id = node_id;
  entry.alive = true;
  entry.last_heartbeat_ms = heartbeat_ms;
}

void HeartbeatManager::OnHeartbeatTimeout(const std::string& node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& entry = nodes_[node_id];
  entry.node_id = node_id;
  entry.alive = false;
}

void HeartbeatManager::IngestGossip(
    const std::vector<NodeHealth>& observations) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& observation : observations) {
    auto& entry = nodes_[observation.node_id];
    if (!entry.node_id.empty() &&
        entry.last_heartbeat_ms > observation.last_heartbeat_ms) {
      continue;
    }
    entry.node_id = observation.node_id;
    entry.alive = observation.alive;
    entry.last_heartbeat_ms = observation.last_heartbeat_ms;
  }
}

std::optional<NodeHealth> HeartbeatManager::GetNodeHealth(
    const std::string& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = nodes_.find(node_id);
  if (found == nodes_.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::vector<NodeHealth> HeartbeatManager::Snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<NodeHealth> snapshot;
  snapshot.reserve(nodes_.size());
  for (const auto& [node_id, health] : nodes_) {
    snapshot.push_back(health);
  }
  return snapshot;
}

bool HeartbeatManager::IsAlive(const std::string& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = nodes_.find(node_id);
  if (found == nodes_.end()) {
    return false;
  }
  return found->second.alive;
}
}  // namespace cache::control
