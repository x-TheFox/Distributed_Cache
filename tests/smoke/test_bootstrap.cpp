#include <gtest/gtest.h>
#include "server/node_config.hpp"

TEST(Bootstrap, DefaultConfigIsValid) {
  auto cfg = cache::server::NodeConfig::Default();
  EXPECT_GT(cfg.shard_count, 0);
  EXPECT_GT(cfg.listen_port, 0);
}
