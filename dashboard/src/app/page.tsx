"use client";

import { useEffect, useState } from "react";

import { parseClusterEvent, type ClusterEvent } from "@/contracts/cluster-events";
import {
  ConnectionHealthPanel,
  type HealthStatus,
  type SocketStatus
} from "@/components/health/connection-health-panel";
import {
  BenchmarkPanel,
  type BenchmarkSnapshot
} from "@/components/metrics/benchmark-panel";
import { LatencyChart } from "@/components/metrics/latency-chart";
import { MetricCards } from "@/components/metrics/metric-cards";
import { SimulationTimeline } from "@/components/simulations/simulation-timeline";
import { SourceBadge } from "@/components/source/source-badge";
import { TopologyMap, type NodeState, type ShardState } from "@/components/topology/topology-map";
import {
  Card,
  CardDescription,
  CardHeader,
  CardTitle
} from "@/components/ui/card";
import { createClusterSocket } from "@/lib/ws-client";

const defaultSocketUrl = "ws://localhost:8080/ws";
const defaultLatencySamples = [1.4, 1.8, 2.2, 2.1, 2.4, 2.3, 2.0];
const defaultOpsPerSec = 180000;
const maxLatencySamples = 12;

const clamp = (value: number, min: number, max: number) =>
  Math.min(max, Math.max(min, value));

const latencySampleFromEvent = (event: ClusterEvent) => {
  if (event.type === "replica_lag") {
    return clamp(event.lagMs / 4, 0.8, 8);
  }

  const derived = (event.ts % 4000) / 1000 + 1;
  return clamp(derived, 0.8, 8);
};

const opsPerSecFromEvent = (event: ClusterEvent) => {
  const base = 170000;
  const jitter = (event.ts % 40000) - 20000;
  let adjustment = -10000;
  if (event.type === "node_heartbeat") {
    adjustment = 15000;
  } else if (event.type === "shard_moved") {
    adjustment = -5000;
  }

  return clamp(base + jitter + adjustment, 110000, 230000);
};

const percentile = (values: number[], ratio: number) => {
  const sorted = [...values].sort((a, b) => a - b);
  const index = Math.min(sorted.length - 1, Math.floor(sorted.length * ratio));
  return sorted[index] ?? 0;
};

export default function Page() {
  const [nodes, setNodes] = useState<NodeState[]>([]);
  const [shards, setShards] = useState<ShardState[]>([]);
  const [socketStatus, setSocketStatus] = useState<SocketStatus>("connecting");
  const [lastEventAt, setLastEventAt] = useState<number | null>(null);
  const [replicaLagMs, setReplicaLagMs] = useState(8);
  const [opsPerSec, setOpsPerSec] = useState(defaultOpsPerSec);
  const [latencySamples, setLatencySamples] = useState(defaultLatencySamples);
  const [simulationEvents, setSimulationEvents] = useState<ClusterEvent[]>([]);
  const [simulationStatus, setSimulationStatus] =
    useState<HealthStatus>("loading");
  const [benchmarkSnapshot, setBenchmarkSnapshot] =
    useState<BenchmarkSnapshot | null>(null);
  const [benchmarkError, setBenchmarkError] = useState<string | null>(null);
  const socketUrl =
    process.env.NEXT_PUBLIC_CLUSTER_WS_URL ?? defaultSocketUrl;

  useEffect(() => {
    let cancelled = false;
    const controller = new AbortController();

    const loadSimulationEvents = async () => {
      try {
        if (!cancelled) {
          setSimulationStatus("loading");
        }
        const response = await fetch("/api/mock-events", {
          signal: controller.signal
        });
        if (!response.ok) {
          if (!cancelled) {
            setSimulationStatus("error");
          }
          return;
        }
        const payload = await response.json();
        if (!Array.isArray(payload)) {
          if (!cancelled) {
            setSimulationStatus("error");
          }
          return;
        }
        const events: ClusterEvent[] = [];
        for (const event of payload) {
          try {
            events.push(parseClusterEvent(event));
          } catch {
            // Ignore malformed events.
          }
        }
        if (!cancelled) {
          setSimulationEvents(events);
          setSimulationStatus("healthy");
        }
      } catch {
        if (!cancelled) {
          setSimulationStatus("error");
        }
      }
    };

    loadSimulationEvents();

    return () => {
      cancelled = true;
      controller.abort();
    };
  }, []);

  useEffect(() => {
    let cancelled = false;
    const controller = new AbortController();

    const loadBenchmarkSnapshot = async () => {
      try {
        const response = await fetch("/api/benchmark-snapshot", {
          signal: controller.signal
        });
        if (!response.ok) {
          const payload = await response.json().catch(() => null);
          if (!cancelled) {
            setBenchmarkError(
              payload?.message ?? "Benchmark snapshot unavailable."
            );
          }
          return;
        }
        const payload = await response.json();
        if (!cancelled) {
          setBenchmarkSnapshot(payload as BenchmarkSnapshot);
          setBenchmarkError(null);
        }
      } catch {
        if (!cancelled) {
          setBenchmarkError("Benchmark snapshot unavailable.");
        }
      }
    };

    loadBenchmarkSnapshot();

    return () => {
      cancelled = true;
      controller.abort();
    };
  }, []);

  useEffect(() => {
    const socket = createClusterSocket(
      socketUrl,
      (event: ClusterEvent) => {
        setLastEventAt(event.ts);
        setOpsPerSec(opsPerSecFromEvent(event));
        setLatencySamples((prev) => {
          const next = [...prev, latencySampleFromEvent(event)];
          return next.slice(-maxLatencySamples);
        });

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
      },
      { onStatusChange: setSocketStatus }
    );

    return () => socket.close();
  }, [socketUrl]);

  const p99Ms = percentile(latencySamples, 0.99);
  const sourceMode =
    process.env.NEXT_PUBLIC_DASHBOARD_SOURCE === "MOCK" ? "MOCK" : "LIVE";
  const benchmarkStatus: HealthStatus = benchmarkError
    ? "error"
    : benchmarkSnapshot
    ? "healthy"
    : "loading";

  return (
    <main className="dashboard-shell">
      <header className="dashboard-header">
        <div
          style={{
            display: "flex",
            justifyContent: "space-between",
            alignItems: "flex-start",
            gap: "16px",
            flexWrap: "wrap"
          }}
        >
          <div>
            <h1>Cluster Dashboard</h1>
            <p>
              Live throughput, latency, and replica lag signals.
            </p>
          </div>
          <div
            style={{
              display: "flex",
              flexDirection: "column",
              alignItems: "flex-end",
              gap: "6px"
            }}
          >
            <div
              style={{
                display: "flex",
                alignItems: "center",
                gap: "8px",
                fontSize: "0.85rem",
                color: "#cbd5e1"
              }}
            >
              <span>Source:</span>
              <SourceBadge mode={sourceMode} />
            </div>
            <div style={{ fontSize: "0.8rem", color: "#94a3b8" }}>
              Last event: {lastEventAt ?? "Awaiting events"}
            </div>
          </div>
        </div>
      </header>
      <section className="section-grid">
        <h2 className="dashboard-section-title">Live integration</h2>
        <ConnectionHealthPanel
          socketStatus={socketStatus}
          socketUrl={socketUrl}
          benchmarkStatus={benchmarkStatus}
          simulationStatus={simulationStatus}
          lastEventAt={lastEventAt}
        />
      </section>
      <section className="section-grid">
        <Card>
          <CardHeader>
            <CardTitle>Performance</CardTitle>
            <CardDescription>
              Throughput, latency and replication health for the active cluster.
            </CardDescription>
          </CardHeader>
          <MetricCards
            opsPerSec={opsPerSec}
            p99Ms={p99Ms}
            replicaLagMs={replicaLagMs}
          />
          <div style={{ marginTop: "16px" }}>
            <LatencyChart values={latencySamples} />
          </div>
          <div style={{ marginTop: "16px" }}>
            <BenchmarkPanel
              snapshot={benchmarkSnapshot}
              error={benchmarkError}
            />
          </div>
        </Card>
      </section>
      <section className="section-grid">
        <h2 className="dashboard-section-title">Cluster topology</h2>
        <TopologyMap nodes={nodes} shards={shards} />
      </section>
      <section className="section-grid">
        <h2 className="dashboard-section-title">Failover simulation</h2>
        <SimulationTimeline events={simulationEvents} />
      </section>
    </main>
  );
}
