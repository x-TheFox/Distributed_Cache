#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "control/heartbeat_manager.hpp"

namespace cache::control {
class RaftMetadataAdapter {
 public:
  void RegisterNode(const std::string& node_id, uint64_t heartbeat_ms = 0);
  void RecordHeartbeat(const std::string& node_id, uint64_t heartbeat_ms);
  void AddShard(const std::string& shard_id,
                const std::vector<std::string>& replicas);
  std::string LeaderForShard(const std::string& shard_id) const;
  void OnHeartbeatTimeout(const std::string& node_id);
  std::optional<NodeHealth> GetNodeHealth(const std::string& node_id) const;

 private:
  struct ShardMetadata {
    std::vector<std::string> replicas;
    std::string leader;
  };

  void ElectLeaderIfNeeded(ShardMetadata& shard);

  HeartbeatManager heartbeat_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, ShardMetadata> shards_;
};
}  // namespace cache::control
