#pragma once

#include <cstddef>
#include <stdexcept>
#include <unordered_map>

#include "replication/replica_stream.hpp"
#include "replication/wal_writer.hpp"

namespace cache::replication {
enum class WriteAckMode { kFastLeaderCommit, kStrongReplicaQuorum };

struct WriteResult {
  size_t log_index{};
  bool acked{};
  size_t leader_commit_index{};
  size_t replica_acks{};
  size_t required_replica_quorum{};
};

class AckPolicy {
 public:
  explicit AckPolicy(size_t replica_count)
      : replica_stream_(replica_count) {}

  WriteResult ApplyWrite(const Mutation& mutation, WriteAckMode mode) {
    const auto log_index = wal_.Append(mutation);
    write_modes_[log_index] = mode;
    const bool leader_committed = wal_.CommitIndex() >= log_index;
    const bool has_replica_quorum =
        replica_stream_.QuorumSize() > 0 && replica_stream_.HasQuorum(log_index);
    acked_[log_index] =
        leader_committed &&
        (mode == WriteAckMode::kFastLeaderCommit ||
         (mode == WriteAckMode::kStrongReplicaQuorum && has_replica_quorum));
    return BuildResult(log_index);
  }

  WriteResult ObserveReplicaAck(size_t replica_id, size_t log_index) {
    const auto mode_it = write_modes_.find(log_index);
    if (mode_it == write_modes_.end()) {
      throw std::invalid_argument("unknown log index");
    }
    replica_stream_.AckReplica(replica_id, log_index);
    if (mode_it->second == WriteAckMode::kStrongReplicaQuorum) {
      const bool has_replica_quorum = replica_stream_.QuorumSize() > 0 &&
                                      replica_stream_.HasQuorum(log_index);
      acked_[log_index] = wal_.CommitIndex() >= log_index && has_replica_quorum;
    }
    return BuildResult(log_index);
  }

 private:
  WriteResult BuildResult(size_t log_index) const {
    const auto acked_it = acked_.find(log_index);
    return WriteResult{
        log_index,
        acked_it != acked_.end() && acked_it->second,
        wal_.CommitIndex(),
        replica_stream_.AckCount(log_index),
        replica_stream_.QuorumSize(),
    };
  }

  WalWriter wal_;
  ReplicaStream replica_stream_;
  std::unordered_map<size_t, WriteAckMode> write_modes_;
  std::unordered_map<size_t, bool> acked_;
};
}  // namespace cache::replication
