# Dashboard event contracts

Baseline cluster event payloads emitted by the control plane for dashboard
telemetry. Each event uses a `type` discriminator alongside required fields.

## node_heartbeat

```json
{
  "type": "node_heartbeat",
  "nodeId": "node-a",
  "ts": 1700000000
}
```

## shard_moved

```json
{
  "type": "shard_moved",
  "shardId": 7,
  "from": "node-a",
  "to": "node-b",
  "ts": 1700000000
}
```

## replica_lag

```json
{
  "type": "replica_lag",
  "shardId": 7,
  "lagMs": 1250,
  "ts": 1700000000
}
```

## dashboard_stream

The dashboard visualizer consumes these payloads over a WebSocket stream. The
default local endpoint is `ws://localhost:8080/ws` and can be overridden with
`NEXT_PUBLIC_CLUSTER_WS_URL` in the dashboard app. For demos and tests, the
dashboard also exposes `GET /api/mock-events` returning an ordered list of the
same event types for the failover simulation timeline.

## Source diagnostics + simulation surfaces

The dashboard highlights which data source is active (`LIVE/MOCK`) and enumerates
health for the supporting diagnostics feeds.

### WebSocket stream (LIVE)

- Source badge reads `LIVE`.
- `ClusterEvent` payloads stream from `ws://localhost:8080/ws` (override via
  `NEXT_PUBLIC_CLUSTER_WS_URL`).

### Mock event replay (MOCK)

- Source badge reads `MOCK`.
- `GET /api/mock-events` returns an ordered array of `ClusterEvent` payloads used
  by the failover timeline and demo playback.

### Scenario catalog

`GET /api/scenarios` returns the available simulation scenarios:

```json
{
  "scenarios": [
    { "id": "thundering_herd", "label": "Thundering Herd" },
    { "id": "coalescing_ab", "label": "Coalescing A/B", "plugin": "herd-lab" }
  ]
}
```

### Benchmark snapshot

`GET /api/benchmark-snapshot` returns the latest perf artifact loaded from
`bench/out/latest.json` (or `BENCHMARK_ARTIFACT_PATH`):

```json
{
  "opsPerSec": 187000,
  "p99Ms": 4.1,
  "metadata": {
    "scenario": "thundering_herd",
    "coalescing": "on"
  }
}
```
