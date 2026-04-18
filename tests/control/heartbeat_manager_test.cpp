#include <gtest/gtest.h>

#include "control/heartbeat_manager.hpp"

namespace cache::control {
TEST(HeartbeatManager, IngestGossipAddsMissingNodes) {
  HeartbeatManager manager;
  manager.RegisterNode("node-a", 100);

  manager.IngestGossip({NodeHealth{"node-b", true, 90}});

  auto health = manager.GetNodeHealth("node-b");
  ASSERT_TRUE(health.has_value());
  EXPECT_TRUE(health->alive);
  EXPECT_EQ(health->last_heartbeat_ms, 90u);
}

TEST(HeartbeatManager, IngestGossipPrefersNewerObservation) {
  HeartbeatManager manager;
  manager.RegisterNode("node-a", 200);

  manager.IngestGossip({NodeHealth{"node-a", false, 150}});
  auto health = manager.GetNodeHealth("node-a");
  ASSERT_TRUE(health.has_value());
  EXPECT_TRUE(health->alive);
  EXPECT_EQ(health->last_heartbeat_ms, 200u);

  manager.IngestGossip({NodeHealth{"node-a", false, 250}});
  health = manager.GetNodeHealth("node-a");
  ASSERT_TRUE(health.has_value());
  EXPECT_FALSE(health->alive);
  EXPECT_EQ(health->last_heartbeat_ms, 250u);
}

TEST(HeartbeatManager, TimeoutBlocksStaleGossipRevival) {
  HeartbeatManager manager;
  manager.RegisterNode("node-a", 100);

  manager.OnHeartbeatTimeout("node-a", 150);
  manager.IngestGossip({NodeHealth{"node-a", true, 120}});

  auto health = manager.GetNodeHealth("node-a");
  ASSERT_TRUE(health.has_value());
  EXPECT_FALSE(health->alive);
  EXPECT_EQ(health->last_heartbeat_ms, 100u);
}

TEST(HeartbeatManager, TimeoutAllowsFreshGossipRevival) {
  HeartbeatManager manager;
  manager.RegisterNode("node-a", 100);

  manager.OnHeartbeatTimeout("node-a", 150);
  manager.IngestGossip({NodeHealth{"node-a", true, 200}});

  auto health = manager.GetNodeHealth("node-a");
  ASSERT_TRUE(health.has_value());
  EXPECT_TRUE(health->alive);
  EXPECT_EQ(health->last_heartbeat_ms, 200u);
}
}  // namespace cache::control
