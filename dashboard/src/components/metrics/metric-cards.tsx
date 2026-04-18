import React from "react";

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
  border: "1px solid #e5e7eb",
  padding: "16px",
  background: "#f9fafb",
  display: "flex",
  flexDirection: "column",
  gap: "8px"
};

const badgeBaseStyle: React.CSSProperties = {
  fontSize: "0.75rem",
  fontWeight: 600,
  padding: "2px 8px",
  borderRadius: "999px",
  textTransform: "uppercase",
  letterSpacing: "0.06em"
};

const formatOps = (value: number) => Math.round(value).toString();
const formatMs = (value: number, digits = 1) => `${value.toFixed(digits)} ms`;

function MetricCard({ label, value, threshold, status }: MetricCardProps) {
  const isWarning = status === "warning";
  const cardStyle: React.CSSProperties = {
    ...cardBaseStyle,
    borderColor: isWarning ? "#f87171" : "#e5e7eb",
    background: isWarning ? "#fef2f2" : "#f9fafb"
  };
  const badgeStyle: React.CSSProperties = {
    ...badgeBaseStyle,
    background: isWarning ? "#fecaca" : "#dcfce7",
    color: isWarning ? "#991b1b" : "#166534"
  };

  return (
    <article style={cardStyle} data-status={status}>
      <div style={{ display: "flex", justifyContent: "space-between", gap: "8px" }}>
        <h3 style={{ margin: 0, fontSize: "0.95rem" }}>{label}</h3>
        <span style={badgeStyle}>{isWarning ? "Warning" : "OK"}</span>
      </div>
      <div style={{ fontSize: "1.6rem", fontWeight: 600 }}>{value}</div>
      <div style={{ fontSize: "0.85rem", color: "#6b7280" }}>{threshold}</div>
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
