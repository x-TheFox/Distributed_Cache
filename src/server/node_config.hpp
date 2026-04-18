#pragma once

#include <string>

namespace cache::server {
struct NodeConfig {
  int shard_count;
  int listen_port;
  int grpc_port;
  int metrics_port;
  std::string node_id;
  int virtual_nodes;
  int max_resp_clients;
  int resp_idle_timeout_ms;
  static NodeConfig Default() {
    return NodeConfig{64, 6379, 50051, 9100, "local", 128, 256, 30000};
  }
};
}  // namespace cache::server
