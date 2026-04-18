#include <cstdlib>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "server/node_config.hpp"

namespace {
std::filesystem::path CacheServerPath() {
  auto candidate = std::filesystem::current_path() / "cache_server";
  if (std::filesystem::exists(candidate)) {
    return candidate;
  }
  return candidate;
}

int RunCacheServer(const std::filesystem::path& path,
                   const std::string& args) {
  std::string command = "\"" + path.string() + "\"";
  if (!args.empty()) {
    command += " " + args;
  }
  return std::system(command.c_str());
}
}  // namespace

TEST(Bootstrap, DefaultConfigIsValid) {
  auto cfg = cache::server::NodeConfig::Default();
  EXPECT_GT(cfg.shard_count, 0);
  EXPECT_GT(cfg.listen_port, 0);
  EXPECT_GT(cfg.grpc_port, 0);
  EXPECT_GT(cfg.metrics_port, 0);
  EXPECT_FALSE(cfg.node_id.empty());
  EXPECT_GT(cfg.virtual_nodes, 0);
}

TEST(Bootstrap, CacheServerDryRunSucceeds) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto result = RunCacheServer(server_path, "--dry-run");
  EXPECT_EQ(result, 0);
}

TEST(Bootstrap, CacheServerBoundedRunSucceeds) {
  auto server_path = CacheServerPath();
  ASSERT_TRUE(std::filesystem::exists(server_path));
  auto result = RunCacheServer(server_path,
                               "--run-for-ms=100 --resp-port=0 --grpc-port=0 "
                               "--metrics-port=0");
  EXPECT_EQ(result, 0);
}
