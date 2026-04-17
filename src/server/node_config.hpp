#pragma once

namespace cache::server {
struct NodeConfig {
  int shard_count;
  int listen_port;
  static NodeConfig Default() { return NodeConfig{64, 6379}; }
};
}  // namespace cache::server
