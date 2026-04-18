#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cache::replication {
struct Mutation {
  std::string key;
  std::string value;
};

class WalWriter {
 public:
  size_t Append(const Mutation& mutation);
  size_t CommitIndex() const;
  size_t Size() const;
  const Mutation& EntryAt(size_t log_index) const;

 private:
  std::vector<Mutation> entries_;
  size_t commit_index_{0};
};
}  // namespace cache::replication
