#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cache::cluster {
class HashRing {
 public:
  void AddNode(std::string node_id, int virtual_nodes);
  void RemoveNode(const std::string& node_id);
  std::string OwnerForKey(const std::string& key) const;

 private:
  static uint64_t Hash64(std::string_view value);
  uint64_t TokenFor(const std::string& node_id, int vnode, int attempt) const;

  std::map<uint64_t, std::string> ring_;
  std::unordered_map<std::string, std::vector<uint64_t>> node_tokens_;
};

std::string RouteKeyToNode(const HashRing& ring, const std::string& key);
}  // namespace cache::cluster
