#include "replication/wal_writer.hpp"

#include <stdexcept>

namespace cache::replication {
size_t WalWriter::Append(const Mutation& mutation) {
  entries_.push_back(mutation);
  commit_index_ = entries_.size();
  return commit_index_;
}

size_t WalWriter::CommitIndex() const { return commit_index_; }

size_t WalWriter::Size() const { return entries_.size(); }

const Mutation& WalWriter::EntryAt(size_t log_index) const {
  if (log_index == 0 || log_index > entries_.size()) {
    throw std::out_of_range("log_index out of range");
  }
  return entries_.at(log_index - 1);
}
}  // namespace cache::replication
