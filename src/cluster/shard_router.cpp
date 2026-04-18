#include "cluster/hash_ring.hpp"

namespace cache::cluster {
std::string RouteKeyToNode(const HashRing& ring, const std::string& key) {
  return ring.OwnerForKey(key);
}
}  // namespace cache::cluster
