#include "control/raft_metadata_adapter.hpp"

namespace cache::control {
void RaftMetadataAdapter::RegisterNode(const std::string& node_id,
                                       uint64_t heartbeat_ms) {
  heartbeat_.RegisterNode(node_id, heartbeat_ms);
}

void RaftMetadataAdapter::RecordHeartbeat(const std::string& node_id,
                                          uint64_t heartbeat_ms) {
  heartbeat_.RecordHeartbeat(node_id, heartbeat_ms);
}

void RaftMetadataAdapter::AddShard(const std::string& shard_id,
                                   const std::vector<std::string>& replicas) {
  std::lock_guard<std::mutex> lock(mutex_);
  ShardMetadata metadata{replicas, replicas.empty() ? std::string()
                                                    : replicas.front()};
  shards_[shard_id] = std::move(metadata);
}

std::string RaftMetadataAdapter::LeaderForShard(
    const std::string& shard_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = shards_.find(shard_id);
  if (found == shards_.end()) {
    return "";
  }
  return found->second.leader;
}

void RaftMetadataAdapter::OnHeartbeatTimeout(const std::string& node_id) {
  heartbeat_.OnHeartbeatTimeout(node_id);
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& [shard_id, shard] : shards_) {
    if (!shard.leader.empty() && heartbeat_.IsAlive(shard.leader)) {
      continue;
    }
    ElectLeaderIfNeeded(shard);
  }
}

std::optional<NodeHealth> RaftMetadataAdapter::GetNodeHealth(
    const std::string& node_id) const {
  return heartbeat_.GetNodeHealth(node_id);
}

void RaftMetadataAdapter::ElectLeaderIfNeeded(ShardMetadata& shard) {
  for (const auto& candidate : shard.replicas) {
    if (heartbeat_.IsAlive(candidate)) {
      shard.leader = candidate;
      return;
    }
  }
  shard.leader.clear();
}
}  // namespace cache::control
