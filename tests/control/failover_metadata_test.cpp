#include <gtest/gtest.h>

#include "control/raft_metadata_adapter.hpp"

namespace cache::control {
TEST(FailoverMetadata, LeaderOwnershipChangesOnHeartbeatLoss) {
  RaftMetadataAdapter metadata;
  metadata.RegisterNode("node-a", 100);
  metadata.RegisterNode("node-b", 100);
  metadata.AddShard("shard-1", {"node-a", "node-b"});

  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-a");

  metadata.OnHeartbeatTimeout("node-a");

  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-b");
  auto health = metadata.GetNodeHealth("node-a");
  ASSERT_TRUE(health.has_value());
  EXPECT_FALSE(health->alive);
}

TEST(FailoverMetadata, LeaderClearsWhenNoHealthyReplica) {
  RaftMetadataAdapter metadata;
  metadata.RegisterNode("node-a", 100);
  metadata.RegisterNode("node-b", 100);
  metadata.AddShard("shard-1", {"node-a", "node-b"});

  metadata.OnHeartbeatTimeout("node-a");
  EXPECT_EQ(metadata.LeaderForShard("shard-1"), "node-b");

  metadata.OnHeartbeatTimeout("node-b");
  EXPECT_TRUE(metadata.LeaderForShard("shard-1").empty());
}
}  // namespace cache::control
