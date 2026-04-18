#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace cache::replication {
class ReplicaStream {
 public:
  explicit ReplicaStream(size_t replica_count);
  void AckReplica(size_t replica_id, size_t log_index);
  size_t AckCount(size_t log_index) const;
  bool HasQuorum(size_t log_index) const;
  size_t QuorumSize() const;

 private:
  size_t replica_count_;
  size_t quorum_size_;
  std::vector<size_t> replica_progress_;
};
}  // namespace cache::replication
