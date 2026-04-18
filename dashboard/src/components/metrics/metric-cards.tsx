import React from "react";

import { Badge } from "@/components/ui/badge";

type MetricCardsProps = {
  opsPerSec: number;
  p99Ms: number;
  replicaLagMs: number;
};

type MetricCardProps = {
  label: string;
  value: string;
  threshold: string;
  status: "ok" | "warning";
};

const cardBaseStyle: React.CSSProperties = {
  borderRadius: "16px",
  border: "1px solid #334155",
  padding: "16px",
  background: "#0f172a",
  display: "flex",
  flexDirection: "column",
  gap: "8px"
};

const formatOps = (value: number) => Math.round(value).toString();
const formatMs = (value: number, digits = 1) => `${value.toFixed(digits)} ms`;

function MetricCard({ label, value, threshold, status }: MetricCardProps) {
  const isWarning = status === "warning";
  const cardStyle: React.CSSProperties = {
    ...cardBaseStyle,
    borderColor: isWarning ? "#b45309" : "#334155",
    background: isWarning ? "#2a1b0a" : "#0f172a"
  };

  return (
    <article style={cardStyle} data-status={status}>
      <div style={{ display: "flex", justifyContent: "space-between", gap: "8px" }}>
        <h3 style={{ margin: 0, fontSize: "0.95rem", color: "#f8fafc" }}>{label}</h3>
        <Badge variant={isWarning ? "warning" : "ok"}>
          {isWarning ? "Warning" : "OK"}
        </Badge>
      </div>
      <div style={{ fontSize: "1.6rem", fontWeight: 600, color: "#e2e8f0" }}>
        {value}
      </div>
      <div style={{ fontSize: "0.85rem", color: "#94a3b8" }}>{threshold}</div>
    </article>
  );
}

export function MetricCards({ opsPerSec, p99Ms, replicaLagMs }: MetricCardsProps) {
  const throughputWarning = opsPerSec < 150000;
  const latencyWarning = p99Ms > 5;
  const replicaLagWarning = replicaLagMs > 20;

  return (
    <section aria-label="Cluster metrics">
      <div
        style={{
          display: "grid",
          gap: "16px",
          gridTemplateColumns: "repeat(auto-fit, minmax(200px, 1fr))"
        }}
      >
        <MetricCard
          label="Throughput"
          value={`${formatOps(opsPerSec)} ops/sec`}
          threshold="Target ≥ 150k ops/sec"
          status={throughputWarning ? "warning" : "ok"}
        />
        <MetricCard
          label="p99 latency"
          value={formatMs(p99Ms, 1)}
          threshold="Target ≤ 5 ms"
          status={latencyWarning ? "warning" : "ok"}
        />
        <MetricCard
          label="Replica Lag"
          value={formatMs(replicaLagMs, 0)}
          threshold="Target ≤ 20 ms"
          status={replicaLagWarning ? "warning" : "ok"}
        />
      </div>
    </section>
  );
}
