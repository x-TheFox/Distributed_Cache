#include <gtest/gtest.h>

#include "replication/ack_policy.hpp"

namespace cache::replication {
TEST(AckPolicy, FastModeAcksAfterLeaderCommit) {
  AckPolicy policy(3);
  Mutation mutation{"k1", "v1"};

  auto result = policy.ApplyWrite(mutation, WriteAckMode::kFastLeaderCommit);

  EXPECT_TRUE(result.acked);
  EXPECT_EQ(result.leader_commit_index, result.log_index);
  EXPECT_EQ(result.replica_acks, 0u);
  EXPECT_EQ(result.required_replica_quorum, 2u);
}

TEST(AckPolicy, StrongModeWaitsForReplicaQuorum) {
  AckPolicy policy(3);
  Mutation mutation{"k2", "v2"};

  auto initial =
      policy.ApplyWrite(mutation, WriteAckMode::kStrongReplicaQuorum);
  EXPECT_FALSE(initial.acked);

  auto after_one = policy.ObserveReplicaAck(0, initial.log_index);
  EXPECT_FALSE(after_one.acked);

  auto after_two = policy.ObserveReplicaAck(1, initial.log_index);
  EXPECT_TRUE(after_two.acked);
  EXPECT_EQ(after_two.replica_acks, 2u);
}
}  // namespace cache::replication
