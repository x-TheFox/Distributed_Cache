#include <stdexcept>
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

TEST(HashRing, RemoveNodeReassignsKeysAwayFromRemovedNode) {
  cache::cluster::HashRing ring;
  ring.AddNode("node-a", 128);
  ring.AddNode("node-b", 128);

  std::string key_on_a;
  auto keys = MakeKeys(1000);
  for (const auto& key : keys) {
    if (ring.OwnerForKey(key) == "node-a") {
      key_on_a = key;
      break;
    }
  }

  ASSERT_FALSE(key_on_a.empty());

  ring.RemoveNode("node-a");

  EXPECT_EQ(ring.OwnerForKey(key_on_a), "node-b");
}

TEST(HashRing, RemoveNodeMissingIsIdempotent) {
  cache::cluster::HashRing ring;
  ring.AddNode("node-a", 64);
  ring.AddNode("node-b", 64);

  auto owner_before = ring.OwnerForKey("alpha");
  ASSERT_FALSE(owner_before.empty());

  ring.RemoveNode("node-missing");

  EXPECT_EQ(ring.OwnerForKey("alpha"), owner_before);
}

TEST(HashRing, AddNodeRejectsInvalidVirtualNodes) {
  cache::cluster::HashRing ring;

  EXPECT_THROW(ring.AddNode("node-a", 0), std::invalid_argument);
  EXPECT_THROW(ring.AddNode("node-a", -3), std::invalid_argument);

  EXPECT_TRUE(ring.OwnerForKey("alpha").empty());
}

TEST(HashRing, RouteKeyToNodeMatchesRingOwner) {
  cache::cluster::HashRing ring;
  ring.AddNode("node-a", 32);
  ring.AddNode("node-b", 32);

  EXPECT_EQ(cache::cluster::RouteKeyToNode(ring, "alpha"),
            ring.OwnerForKey("alpha"));
}

TEST(HashRing, RouteKeyToNodeReturnsEmptyWhenRingEmpty) {
  cache::cluster::HashRing ring;

  EXPECT_TRUE(cache::cluster::RouteKeyToNode(ring, "alpha").empty());
}

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
