#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cluster/hash_ring.hpp"

namespace {
std::vector<std::string> MakeKeys(size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    keys.push_back("key_" + std::to_string(i));
  }
  return keys;
}
}  // namespace

TEST(HashRing, OwnerForKeyIsDeterministic) {
  cache::cluster::HashRing ring;
  ring.AddNode("node-a", 64);
  ring.AddNode("node-b", 64);

  auto owner1 = ring.OwnerForKey("alpha");
  auto owner2 = ring.OwnerForKey("alpha");

  EXPECT_FALSE(owner1.empty());
  EXPECT_EQ(owner1, owner2);
}

TEST(HashRing, AddingNodeMovesMinorityOfKeys) {
  cache::cluster::HashRing ring;
  ring.AddNode("node-a", 128);
  ring.AddNode("node-b", 128);
  ring.AddNode("node-c", 128);

  constexpr size_t kKeyCount = 5000;
  auto keys = MakeKeys(kKeyCount);
  std::vector<std::string> before;
  before.reserve(keys.size());
  for (const auto& key : keys) {
    before.push_back(ring.OwnerForKey(key));
  }

  ring.AddNode("node-d", 128);

  size_t moved = 0;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (ring.OwnerForKey(keys[i]) != before[i]) {
      ++moved;
    }
  }

  const double ratio = static_cast<double>(moved) /
                       static_cast<double>(keys.size());
  EXPECT_GT(ratio, 0.05);
  EXPECT_LT(ratio, 0.35);
}
