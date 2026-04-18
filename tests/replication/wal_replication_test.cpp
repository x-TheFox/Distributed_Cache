#include <gtest/gtest.h>

#include "replication/replica_stream.hpp"
#include "replication/wal_writer.hpp"

namespace cache::replication {
TEST(WALReplication, AppendAssignsSequentialIndexAndCommits) {
  WalWriter wal;
  Mutation first{"alpha", "1"};
  Mutation second{"bravo", "2"};

  auto index1 = wal.Append(first);
  auto index2 = wal.Append(second);

  EXPECT_EQ(index1, 1u);
  EXPECT_EQ(index2, 2u);
  EXPECT_EQ(wal.CommitIndex(), 2u);
  EXPECT_EQ(wal.Size(), 2u);
  EXPECT_EQ(wal.EntryAt(1).key, "alpha");
  EXPECT_EQ(wal.EntryAt(2).value, "2");
}

TEST(WALReplication, ReplicaStreamTracksQuorumAcks) {
  ReplicaStream stream(3);
  EXPECT_EQ(stream.QuorumSize(), 2u);

  stream.AckReplica(0, 1);
  EXPECT_FALSE(stream.HasQuorum(1));

  stream.AckReplica(1, 1);
  EXPECT_TRUE(stream.HasQuorum(1));
  EXPECT_EQ(stream.AckCount(1), 2u);
}
}  // namespace cache::replication
