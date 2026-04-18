import React from "react";

export type BenchmarkSnapshot = {
  opsPerSec: number;
  p99Ms: number;
  metadata?: Record<string, unknown>;
};

type BenchmarkPanelProps = {
  snapshot: BenchmarkSnapshot | null;
  error?: string | null;
};

const panelStyle: React.CSSProperties = {
  borderRadius: "16px",
  border: "1px solid #e5e7eb",
  padding: "16px",
  background: "#ffffff",
  display: "flex",
  flexDirection: "column",
  gap: "16px"
};

const statsGridStyle: React.CSSProperties = {
  display: "grid",
  gap: "12px",
  gridTemplateColumns: "repeat(auto-fit, minmax(160px, 1fr))"
};

const statStyle: React.CSSProperties = {
  display: "flex",
  flexDirection: "column",
  gap: "4px"
};

const labelStyle: React.CSSProperties = {
  fontSize: "0.85rem",
  color: "#6b7280",
  textTransform: "uppercase",
  letterSpacing: "0.05em"
};

const valueStyle: React.CSSProperties = {
  fontSize: "1.4rem",
  fontWeight: 600
};

const formatOps = (value: number) => Math.round(value).toString();
const formatMs = (value: number) =>
  `${value.toFixed(value < 1 ? 3 : 1)} ms`;

const formatMetadataLabel = (value: string) =>
  value
    .replace(/_/g, " ")
    .replace(/([a-z])([A-Z])/g, "$1 $2")
    .replace(/\b\w/g, (char) => char.toUpperCase());

const formatMetadataValue = (value: unknown) => {
  if (typeof value === "number") {
    return value.toString();
  }
  if (typeof value === "string") {
    return value;
  }
  if (value === null) {
    return "null";
  }
  return JSON.stringify(value);
};

export function BenchmarkPanel({ snapshot, error }: BenchmarkPanelProps) {
  if (error) {
    return (
      <section aria-label="Benchmark snapshot" style={panelStyle}>
        <h3 style={{ margin: 0 }}>Latest benchmark</h3>
        <p style={{ margin: 0, color: "#b91c1c" }}>{error}</p>
      </section>
    );
  }

  if (!snapshot) {
    return (
      <section aria-label="Benchmark snapshot" style={panelStyle}>
        <h3 style={{ margin: 0 }}>Latest benchmark</h3>
        <p style={{ margin: 0, color: "#6b7280" }}>Loading benchmark snapshot...</p>
      </section>
    );
  }

  const metadataEntries = snapshot.metadata
    ? Object.entries(snapshot.metadata).sort(([a], [b]) =>
        a.localeCompare(b)
      )
    : [];

  return (
    <section aria-label="Benchmark snapshot" style={panelStyle}>
      <h3 style={{ margin: 0 }}>Latest benchmark</h3>
      <div style={statsGridStyle}>
        <div style={statStyle}>
          <span style={labelStyle}>Ops/sec</span>
          <span style={valueStyle}>{formatOps(snapshot.opsPerSec)} ops/sec</span>
        </div>
        <div style={statStyle}>
          <span style={labelStyle}>p99</span>
          <span style={valueStyle}>{formatMs(snapshot.p99Ms)}</span>
        </div>
      </div>
      {metadataEntries.length > 0 ? (
        <dl style={{ margin: 0, display: "grid", gap: "8px" }}>
          {metadataEntries.map(([key, value]) => (
            <div
              key={key}
              style={{
                display: "flex",
                justifyContent: "space-between",
                gap: "12px"
              }}
            >
              <dt style={{ fontWeight: 600 }}>{formatMetadataLabel(key)}</dt>
              <dd style={{ margin: 0, color: "#374151" }}>
                {formatMetadataValue(value)}
              </dd>
            </div>
          ))}
        </dl>
      ) : null}
    </section>
  );
}
