#include "replication/replica_stream.hpp"

#include <stdexcept>

namespace cache::replication {
ReplicaStream::ReplicaStream(size_t replica_count)
    : replica_count_(replica_count),
      quorum_size_(replica_count == 0 ? 0 : replica_count / 2 + 1),
      replica_progress_(replica_count, 0) {}

void ReplicaStream::AckReplica(size_t replica_id, size_t log_index) {
  if (replica_id >= replica_count_) {
    throw std::out_of_range("replica_id out of range");
  }
  if (log_index > replica_progress_[replica_id]) {
    replica_progress_[replica_id] = log_index;
  }
}

size_t ReplicaStream::AckCount(size_t log_index) const {
  size_t count = 0;
  for (auto progress : replica_progress_) {
    if (progress >= log_index) {
      ++count;
    }
  }
  return count;
}

bool ReplicaStream::HasQuorum(size_t log_index) const {
  return AckCount(log_index) >= quorum_size_;
}

size_t ReplicaStream::QuorumSize() const { return quorum_size_; }
}  // namespace cache::replication
