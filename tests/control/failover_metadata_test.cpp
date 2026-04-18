#include <gtest/gtest.h>

#include "control/raft_metadata_adapter.hpp"

namespace cache::control {
TEST(FailoverMetadata, LeaderOwnershipChangesOnHeartbeatLoss) {
  RaftMetadataAdapter metadata;
  metadata.RegisterNode("node-a", 100);
  metadata.RegisterNode("node-b", 100);
  metadata.AddShard("shard-1", {"node-a", "node-b"});

  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-a");

  metadata.OnHeartbeatTimeout("node-a", 200);

  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-b");
  auto health = metadata.GetNodeHealth("node-a");
  ASSERT_TRUE(health.has_value());
  EXPECT_FALSE(health->alive);
}

TEST(FailoverMetadata, AddShardSelectsHealthyReplica) {
  RaftMetadataAdapter metadata;
  metadata.RegisterNode("node-a", 100);
  metadata.RegisterNode("node-b", 100);
  metadata.OnHeartbeatTimeout("node-a", 200);

  metadata.AddShard("shard-1", {"node-a", "node-b"});

  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-b");
}

TEST(FailoverMetadata, AddShardLeavesLeaderEmptyWhenAllReplicasDead) {
  RaftMetadataAdapter metadata;
  metadata.RegisterNode("node-a", 100);
  metadata.RegisterNode("node-b", 100);
  metadata.OnHeartbeatTimeout("node-a", 200);
  metadata.OnHeartbeatTimeout("node-b", 200);

  metadata.AddShard("shard-1", {"node-a", "node-b"});

  EXPECT_TRUE(metadata.LeaderForShard("shard-1").empty());
}

TEST(FailoverMetadata, LeaderClearsWhenNoHealthyReplica) {
  RaftMetadataAdapter metadata;
  metadata.RegisterNode("node-a", 100);
  metadata.RegisterNode("node-b", 100);
  metadata.AddShard("shard-1", {"node-a", "node-b"});

  metadata.OnHeartbeatTimeout("node-a", 200);
  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-b");

  metadata.OnHeartbeatTimeout("node-b", 250);
  EXPECT_TRUE(metadata.LeaderForShard("shard-1").empty());
}

TEST(FailoverMetadata, GossipRevivesReplicaForNextFailover) {
  RaftMetadataAdapter metadata;
  metadata.RegisterNode("node-a", 100);
  metadata.RegisterNode("node-b", 100);
  metadata.AddShard("shard-1", {"node-a", "node-b"});

  metadata.OnHeartbeatTimeout("node-a", 200);
  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-b");

  metadata.IngestGossip({NodeHealth{"node-a", true, 250}});
  auto health = metadata.GetNodeHealth("node-a");
  ASSERT_TRUE(health.has_value());
  EXPECT_TRUE(health->alive);

  metadata.OnHeartbeatTimeout("node-b", 300);
  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-a");
}

TEST(FailoverMetadata, StaleTimeoutDoesNotTriggerFailover) {
  RaftMetadataAdapter metadata;
  metadata.RegisterNode("node-a", 100);
  metadata.RegisterNode("node-b", 100);
  metadata.AddShard("shard-1", {"node-a", "node-b"});

  metadata.RecordHeartbeat("node-a", 300);
  metadata.OnHeartbeatTimeout("node-a", 200);

  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-a");
  auto health = metadata.GetNodeHealth("node-a");
  ASSERT_TRUE(health.has_value());
  EXPECT_TRUE(health->alive);
}
}  // namespace cache::control
