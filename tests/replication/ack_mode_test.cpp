#include <gtest/gtest.h>

#include <stdexcept>

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

TEST(AckPolicy, StrongModeWithZeroReplicasDoesNotAutoAck) {
  AckPolicy policy(0);
  Mutation mutation{"k3", "v3"};

  auto result = policy.ApplyWrite(mutation, WriteAckMode::kStrongReplicaQuorum);

  EXPECT_FALSE(result.acked);
  EXPECT_EQ(result.replica_acks, 0u);
  EXPECT_EQ(result.required_replica_quorum, 0u);
}

TEST(AckPolicy, InvalidLogIndexDoesNotAdvanceReplicaProgress) {
  AckPolicy policy(1);
  Mutation mutation{"k4", "v4"};

  auto initial =
      policy.ApplyWrite(mutation, WriteAckMode::kStrongReplicaQuorum);

  EXPECT_FALSE(initial.acked);
  EXPECT_THROW(policy.ObserveReplicaAck(0, initial.log_index + 1),
               std::invalid_argument);

  Mutation next{"k5", "v5"};
  auto after_invalid =
      policy.ApplyWrite(next, WriteAckMode::kStrongReplicaQuorum);
  EXPECT_FALSE(after_invalid.acked);
  EXPECT_EQ(after_invalid.replica_acks, 0u);
}
}  // namespace cache::replication
