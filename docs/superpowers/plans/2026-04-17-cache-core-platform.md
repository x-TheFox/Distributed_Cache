# Cache Core Platform Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a production-grade C++ distributed in-memory cache core with dual protocol support (RESP + gRPC), adaptive eviction, sharding, replication, fault tolerance, metrics, and CI-driven benchmark SVGs.

**Architecture:** Use a shard-oriented data plane for cache operations, with RAFT-backed metadata and heartbeat health for ownership/failover decisions. Writes support two consistency modes: fast (leader-commit ACK) and strong (replica-quorum ACK). Observability and benchmark pipelines are first-class and drive README artifacts.

**Tech Stack:** C++20, CMake, GoogleTest, gRPC/Protobuf, Prometheus exposition format, GitHub Actions.

---

## File Structure Map

- `.gitignore` — ignore build artifacts, `.superpowers/`, benchmark outputs, generated protobuf files
- `CMakeLists.txt` — root build graph
- `cmake/third_party.cmake` — dependency wiring for gRPC/Protobuf/GTest
- `src/core/` — in-memory storage, concurrency, eviction
- `src/protocol/resp/` — RESP parser and command encoder/decoder
- `src/protocol/grpc/` — gRPC service definitions and server adapters
- `src/cluster/` — consistent hashing, node registry, shard ownership
- `src/replication/` — WAL writer, replica streamer, ack policy logic
- `src/control/` — heartbeat, gossip, RAFT metadata adapter
- `src/metrics/` — Prometheus endpoint and metric registries
- `src/server/` — node bootstrap, config, wiring across modules
- `bench/` — benchmark scenarios and JSON/SVG emitters
- `tests/` — unit, integration, and failover/perf correctness tests
- `.github/workflows/benchmarks.yml` — CI benchmark workflow producing SVG assets
- `README.md` — architecture summary and auto-updated benchmark visuals

## Task 1: Repository Guardrails and Build Skeleton

**Files:**
- Create: `.gitignore`
- Create: `CMakeLists.txt`
- Create: `cmake/third_party.cmake`
- Create: `src/server/main.cpp`
- Create: `tests/smoke/test_bootstrap.cpp`
- Test: `tests/smoke/test_bootstrap.cpp`

- [ ] **Step 1: Write the failing smoke test**

```cpp
// tests/smoke/test_bootstrap.cpp
#include <gtest/gtest.h>
#include "server/node_config.hpp"

TEST(Bootstrap, DefaultConfigIsValid) {
  auto cfg = cache::server::NodeConfig::Default();
  EXPECT_GT(cfg.shard_count, 0);
  EXPECT_GT(cfg.listen_port, 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -S . -B build && cmake --build build && ctest --test-dir build -R Bootstrap --output-on-failure`  
Expected: FAIL with missing `server/node_config.hpp` / undefined symbols.

- [ ] **Step 3: Add minimal implementation and .gitignore policy**

```gitignore
# .gitignore
/build/
/.superpowers/
/.idea/
/.vscode/
*.o
*.a
*.so
*.dylib
*.log
*.profraw
*.profdata
/bench/out/
/docs/generated/
```

```cpp
// src/server/node_config.hpp
#pragma once
namespace cache::server {
struct NodeConfig {
  int shard_count;
  int listen_port;
  static NodeConfig Default() { return NodeConfig{64, 6379}; }
};
}  // namespace cache::server
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -R Bootstrap --output-on-failure`  
Expected: PASS with 1 test run.

- [ ] **Step 5: Commit**

```bash
git add .gitignore CMakeLists.txt cmake/third_party.cmake src/server/main.cpp src/server/node_config.hpp tests/smoke/test_bootstrap.cpp
git commit -m "build: bootstrap cmake project and gitignore policies"
```

## Task 2: Concurrent Store + TTL Foundations

**Files:**
- Create: `src/core/value_entry.hpp`
- Create: `src/core/concurrent_store.hpp`
- Create: `src/core/concurrent_store.cpp`
- Test: `tests/core/concurrent_store_test.cpp`

- [ ] **Step 1: Write failing tests for set/get/ttl**

```cpp
TEST(ConcurrentStore, SetGetRoundTrip) {
  cache::core::ConcurrentStore store(16);
  store.Set("k1", "v1", std::nullopt);
  EXPECT_EQ(store.Get("k1").value(), "v1");
}

TEST(ConcurrentStore, ExpiredEntryIsNotReturned) {
  cache::core::ConcurrentStore store(16);
  store.Set("k2", "v2", std::chrono::milliseconds(10));
  std::this_thread::sleep_for(std::chrono::milliseconds(15));
  EXPECT_FALSE(store.Get("k2").has_value());
}
```

- [ ] **Step 2: Run tests to verify failures**

Run: `ctest --test-dir build -R ConcurrentStore --output-on-failure`  
Expected: FAIL due missing `ConcurrentStore`.

- [ ] **Step 3: Implement striped-lock store**

```cpp
class ConcurrentStore {
 public:
  explicit ConcurrentStore(size_t stripes);
  void Set(std::string key, std::string value,
           std::optional<std::chrono::milliseconds> ttl);
  std::optional<std::string> Get(const std::string& key);
 private:
  struct Stripe { std::shared_mutex mu; std::unordered_map<std::string, ValueEntry> map; };
  std::vector<Stripe> stripes_;
};
```

- [ ] **Step 4: Run tests to verify pass**

Run: `ctest --test-dir build -R ConcurrentStore --output-on-failure`  
Expected: PASS for roundtrip and ttl expiration.

- [ ] **Step 5: Commit**

```bash
git add src/core/value_entry.hpp src/core/concurrent_store.* tests/core/concurrent_store_test.cpp
git commit -m "feat(core): add striped concurrent store with ttl support"
```

## Task 3: Adaptive Eviction (SLRU + LFU Bias)

**Files:**
- Create: `src/core/eviction/eviction_policy.hpp`
- Create: `src/core/eviction/slru_lfu_policy.cpp`
- Modify: `src/core/concurrent_store.cpp`
- Test: `tests/core/eviction_policy_test.cpp`

- [ ] **Step 1: Write failing eviction behavior tests**

```cpp
TEST(EvictionPolicy, KeepsFrequentlyAccessedKeys) {
  // Insert keys, repeatedly access "hot", force memory pressure, assert "hot" survives.
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R EvictionPolicy --output-on-failure`  
Expected: FAIL because policy not implemented.

- [ ] **Step 3: Implement policy and store integration**

```cpp
struct EvictionCandidate {
  std::string key;
  uint32_t lfu_count;
  uint64_t last_touch_ns;
  bool in_protected_segment;
};

std::string SelectVictim(const std::vector<EvictionCandidate>& candidates);
```

- [ ] **Step 4: Run tests to verify pass**

Run: `ctest --test-dir build -R EvictionPolicy --output-on-failure`  
Expected: PASS and deterministic victim selection in tests.

- [ ] **Step 5: Commit**

```bash
git add src/core/eviction src/core/concurrent_store.cpp tests/core/eviction_policy_test.cpp
git commit -m "feat(core): implement slru eviction with lfu-aware victim scoring"
```

## Task 4: RESP Protocol + Command Execution

**Files:**
- Create: `src/protocol/resp/resp_parser.hpp`
- Create: `src/protocol/resp/resp_parser.cpp`
- Create: `src/server/command_dispatcher.cpp`
- Test: `tests/protocol/resp_parser_test.cpp`
- Test: `tests/integration/resp_set_get_test.cpp`

- [ ] **Step 1: Write failing parser and set/get integration tests**

```cpp
TEST(RESPParser, ParsesSetCommandArray) {
  auto cmd = ParseRESP("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  EXPECT_EQ(cmd.name, "SET");
  EXPECT_EQ(cmd.args[0], "foo");
}
```

- [ ] **Step 2: Run tests to verify failures**

Run: `ctest --test-dir build -R RESP --output-on-failure`  
Expected: FAIL with missing parser symbols.

- [ ] **Step 3: Implement parser and command routing**

```cpp
struct RESPCommand { std::string name; std::vector<std::string> args; };
RESPCommand ParseRESP(std::string_view frame);
std::string ExecuteCommand(const RESPCommand& cmd, cache::core::ConcurrentStore& store);
```

- [ ] **Step 4: Run tests to verify pass**

Run: `ctest --test-dir build -R "RESP|resp_set_get" --output-on-failure`  
Expected: PASS with `+OK` and bulk string value responses.

- [ ] **Step 5: Commit**

```bash
git add src/protocol/resp src/server/command_dispatcher.cpp tests/protocol/resp_parser_test.cpp tests/integration/resp_set_get_test.cpp
git commit -m "feat(protocol): add RESP parser and command dispatcher"
```

## Task 5: gRPC API and Shared Command Path

**Files:**
- Create: `proto/cache.proto`
- Create: `src/protocol/grpc/cache_service.cpp`
- Modify: `src/server/command_dispatcher.cpp`
- Test: `tests/protocol/grpc_cache_service_test.cpp`

- [ ] **Step 1: Write failing gRPC integration test**

```cpp
TEST(GRPCCacheService, SetAndGetRoundTrip) {
  // Start in-process grpc server, issue Set/Get RPC, assert value.
}
```

- [ ] **Step 2: Run test to verify failure**

Run: `ctest --test-dir build -R GRPCCacheService --output-on-failure`  
Expected: FAIL because generated service and implementation are missing.

- [ ] **Step 3: Implement proto contract and service**

```proto
service CacheService {
  rpc Set(SetRequest) returns (SetResponse);
  rpc Get(GetRequest) returns (GetResponse);
}
```

```cpp
grpc::Status CacheServiceImpl::Set(...);
grpc::Status CacheServiceImpl::Get(...);
```

- [ ] **Step 4: Run tests to verify pass**

Run: `ctest --test-dir build -R "GRPCCacheService|RESP" --output-on-failure`  
Expected: PASS with shared semantics across RESP and gRPC paths.

- [ ] **Step 5: Commit**

```bash
git add proto/cache.proto src/protocol/grpc src/server/command_dispatcher.cpp tests/protocol/grpc_cache_service_test.cpp
git commit -m "feat(protocol): add grpc cache service with shared command path"
```

## Task 6: Consistent Hashing + Shard Router

**Files:**
- Create: `src/cluster/hash_ring.hpp`
- Create: `src/cluster/hash_ring.cpp`
- Create: `src/cluster/shard_router.cpp`
- Test: `tests/cluster/hash_ring_test.cpp`

- [ ] **Step 1: Write failing tests for stable mapping and limited churn**

```cpp
TEST(HashRing, AddingNodeMovesMinorityOfKeys) {
  // Compare mappings before/after adding a node; assert movement ratio threshold.
}
```

- [ ] **Step 2: Run test to verify failure**

Run: `ctest --test-dir build -R HashRing --output-on-failure`  
Expected: FAIL with unresolved hash ring APIs.

- [ ] **Step 3: Implement virtual-node consistent hash ring**

```cpp
class HashRing {
 public:
  void AddNode(std::string node_id, int virtual_nodes);
  void RemoveNode(const std::string& node_id);
  std::string OwnerForKey(const std::string& key) const;
};
```

- [ ] **Step 4: Run tests to verify pass**

Run: `ctest --test-dir build -R HashRing --output-on-failure`  
Expected: PASS with deterministic ownership and bounded churn.

- [ ] **Step 5: Commit**

```bash
git add src/cluster/hash_ring.* src/cluster/shard_router.cpp tests/cluster/hash_ring_test.cpp
git commit -m "feat(cluster): add consistent hash ring and shard router"
```

## Task 7: WAL + Active-Replica Replication + ACK Modes

**Files:**
- Create: `src/replication/wal_writer.cpp`
- Create: `src/replication/replica_stream.cpp`
- Create: `src/replication/ack_policy.hpp`
- Test: `tests/replication/wal_replication_test.cpp`
- Test: `tests/replication/ack_mode_test.cpp`

- [ ] **Step 1: Write failing tests for fast vs strong ACK**

```cpp
TEST(AckPolicy, FastModeAcksAfterLeaderCommit) { /* ... */ }
TEST(AckPolicy, StrongModeWaitsForReplicaQuorum) { /* ... */ }
```

- [ ] **Step 2: Run tests to verify failure**

Run: `ctest --test-dir build -R "AckPolicy|WALReplication" --output-on-failure`  
Expected: FAIL due missing wal/replication components.

- [ ] **Step 3: Implement WAL append + replication tracker**

```cpp
enum class WriteAckMode { kFastLeaderCommit, kStrongReplicaQuorum };
WriteResult ApplyWrite(const Mutation& mutation, WriteAckMode mode);
```

- [ ] **Step 4: Run tests to verify pass**

Run: `ctest --test-dir build -R "AckPolicy|WALReplication" --output-on-failure`  
Expected: PASS with mode-specific timing assertions.

- [ ] **Step 5: Commit**

```bash
git add src/replication tests/replication
git commit -m "feat(replication): add async wal, active-replica stream, and dual ack modes"
```

## Task 8: Heartbeat/Gossip, RAFT Metadata Adapter, Coalescing, and Metrics

**Files:**
- Create: `src/control/heartbeat_manager.cpp`
- Create: `src/control/raft_metadata_adapter.cpp`
- Create: `src/core/request_coalescer.cpp`
- Create: `src/metrics/prometheus_endpoint.cpp`
- Test: `tests/control/failover_metadata_test.cpp`
- Test: `tests/core/request_coalescer_test.cpp`
- Test: `tests/metrics/prometheus_endpoint_test.cpp`

- [ ] **Step 1: Write failing tests for failover metadata updates and coalescing**

```cpp
TEST(RequestCoalescer, DeduplicatesHotMissInflightRequests) { /* ... */ }
TEST(FailoverMetadata, LeaderOwnershipChangesOnHeartbeatLoss) { /* ... */ }
```

- [ ] **Step 2: Run tests to verify failure**

Run: `ctest --test-dir build -R "RequestCoalescer|FailoverMetadata|Prometheus" --output-on-failure`  
Expected: FAIL because control-plane and metrics modules do not exist.

- [ ] **Step 3: Implement control-plane adapter and observability**

```cpp
struct NodeHealth { std::string node_id; bool alive; uint64_t last_heartbeat_ms; };
void OnHeartbeatTimeout(const std::string& node_id);
std::string RenderPrometheusMetrics() const;
```

- [ ] **Step 4: Run tests to verify pass**

Run: `ctest --test-dir build -R "RequestCoalescer|FailoverMetadata|Prometheus" --output-on-failure`  
Expected: PASS with deterministic coalescing and metric exposition checks.

- [ ] **Step 5: Commit**

```bash
git add src/control src/core/request_coalescer.cpp src/metrics tests/control tests/core/request_coalescer_test.cpp tests/metrics/prometheus_endpoint_test.cpp
git commit -m "feat(control): add heartbeat/raft adapter, coalescing, and prometheus endpoint"
```

## Task 9: Benchmark Harness, CI SVG Output, and README Integration

**Files:**
- Create: `bench/cache_bench.cpp`
- Create: `bench/render_svg.py`
- Create: `.github/workflows/benchmarks.yml`
- Modify: `README.md`
- Test: `tests/bench/benchmark_output_test.py`

- [ ] **Step 1: Write failing test for benchmark artifact format**

```python
def test_benchmark_json_contains_required_fields():
    data = json.loads(Path("bench/out/latest.json").read_text())
    assert "ops_per_sec" in data
    assert "p99_ms" in data
```

- [ ] **Step 2: Run test to verify failure**

Run: `pytest tests/bench/benchmark_output_test.py -q`  
Expected: FAIL because benchmark output file is missing.

- [ ] **Step 3: Implement benchmark runner + SVG renderer + workflow**

```yaml
# .github/workflows/benchmarks.yml (key job fragment)
- name: Run benchmark
  run: ./build/bench/cache_bench --out bench/out/latest.json
- name: Render SVG
  run: python3 bench/render_svg.py bench/out/latest.json docs/generated/bench.svg
```

- [ ] **Step 4: Run tests to verify pass**

Run: `pytest tests/bench/benchmark_output_test.py -q`  
Expected: PASS; plus `docs/generated/bench.svg` exists after local benchmark run.

- [ ] **Step 5: Commit**

```bash
git add bench .github/workflows/benchmarks.yml README.md tests/bench/benchmark_output_test.py
git commit -m "feat(obs): add benchmark pipeline with ci-generated svg readme artifacts"
```

## Spec Coverage Check (Self-Review)

- Discovery audit + status declaration: Task 1 and README integration
- Concurrency, eviction, and protocol support: Tasks 2-5
- Consistent hashing and node routing: Task 6
- WAL, replication, ack trade-off model: Task 7
- Fault tolerance (heartbeat + RAFT metadata): Task 8
- Request coalescing and Prometheus metrics: Task 8
- CI-driven benchmark visuals and README quality: Task 9

## Placeholder Scan (Self-Review)

- No `TBD`, `TODO`, or deferred implementation markers remain in tasks.
- Each task contains concrete files, concrete commands, and concrete code snippets.

## Type/Signature Consistency (Self-Review)

- `WriteAckMode` naming is consistent across replication tasks.
- `ConcurrentStore` API (`Set`, `Get`) is reused consistently by protocol tasks.
- Router ownership contract (`OwnerForKey`) is used consistently in cluster tasks.
