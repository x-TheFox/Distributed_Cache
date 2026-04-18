"use client";

import { useEffect, useState } from "react";

import type { ClusterEvent } from "@/contracts/cluster-events";
import { LatencyChart } from "@/components/metrics/latency-chart";
import { MetricCards } from "@/components/metrics/metric-cards";
import { TopologyMap, type NodeState, type ShardState } from "@/components/topology/topology-map";
import { createClusterSocket } from "@/lib/ws-client";

const defaultSocketUrl = "ws://localhost:8080/ws";
const defaultLatencySamples = [1.4, 1.8, 2.2, 2.1, 2.4, 2.3, 2.0];
const defaultOpsPerSec = 180000;

const percentile = (values: number[], ratio: number) => {
  const sorted = [...values].sort((a, b) => a - b);
  const index = Math.min(sorted.length - 1, Math.floor(sorted.length * ratio));
  return sorted[index] ?? 0;
};

export default function Page() {
  const [nodes, setNodes] = useState<NodeState[]>([]);
  const [shards, setShards] = useState<ShardState[]>([]);
  const [replicaLagMs, setReplicaLagMs] = useState(8);

  useEffect(() => {
    const socketUrl =
      process.env.NEXT_PUBLIC_CLUSTER_WS_URL ?? defaultSocketUrl;

    const socket = createClusterSocket(socketUrl, (event: ClusterEvent) => {
      if (event.type === "node_heartbeat") {
        setNodes((prev) => {
          const index = prev.findIndex((node) => node.id === event.nodeId);
          if (index === -1) {
            return [...prev, { id: event.nodeId, alive: true }];
          }

          const next = [...prev];
          next[index] = { ...next[index], alive: true };
          return next;
        });
        return;
      }

      if (event.type === "replica_lag") {
        setReplicaLagMs(event.lagMs);
        return;
      }

      if (event.type === "shard_moved") {
        setShards((prev) => {
          const index = prev.findIndex((shard) => shard.id === event.shardId);
          if (index === -1) {
            return [...prev, { id: event.shardId, owner: event.to }];
          }

          const next = [...prev];
          next[index] = { ...next[index], owner: event.to };
          return next;
        });
      }
    });

    return () => socket.close();
  }, []);

  const p99Ms = percentile(defaultLatencySamples, 0.99);

  return (
    <main
      style={{
        display: "flex",
        flexDirection: "column",
        gap: "32px",
        padding: "32px"
      }}
    >
      <header>
        <h1>Cluster Dashboard</h1>
        <p style={{ margin: 0, color: "#6b7280" }}>
          Live throughput, latency, and replica lag signals.
        </p>
      </header>
      <section>
        <h2>Performance</h2>
        <MetricCards
          opsPerSec={defaultOpsPerSec}
          p99Ms={p99Ms}
          replicaLagMs={replicaLagMs}
        />
        <div style={{ marginTop: "16px" }}>
          <LatencyChart values={defaultLatencySamples} />
        </div>
      </section>
      <section>
        <h2>Cluster Topology</h2>
        <TopologyMap nodes={nodes} shards={shards} />
      </section>
    </main>
  );
}
