import React from "react";

import { Badge } from "@/components/ui/badge";

export type SocketStatus = "connected" | "connecting" | "reconnecting" | "disconnected";
export type HealthStatus = "healthy" | "loading" | "error";

type ConnectionHealthPanelProps = {
  socketStatus: SocketStatus;
  socketUrl: string;
  benchmarkStatus: HealthStatus;
  simulationStatus: HealthStatus;
  lastEventAt?: number | null;
  benchmarkDetail?: string;
  simulationDetail?: string;
};

type StatusBadge = {
  label: string;
  variant: "ok" | "warning" | "danger";
};

const panelStyle: React.CSSProperties = {
  border: "1px solid #334155",
  borderRadius: "16px",
  padding: "16px",
  background: "#0f172a",
  display: "grid",
  gap: "16px"
};

const listStyle: React.CSSProperties = {
  listStyle: "none",
  padding: 0,
  margin: 0,
  display: "grid",
  gap: "12px"
};

const itemStyle: React.CSSProperties = {
  border: "1px solid #334155",
  borderRadius: "12px",
  padding: "12px",
  background: "#111c34",
  display: "flex",
  flexDirection: "column",
  gap: "6px"
};

const rowStyle: React.CSSProperties = {
  display: "flex",
  justifyContent: "space-between",
  alignItems: "center",
  gap: "12px"
};

const detailStyle: React.CSSProperties = {
  fontSize: "0.8rem",
  color: "#94a3b8"
};

const socketBadgeMap: Record<SocketStatus, StatusBadge> = {
  connected: { label: "Connected", variant: "ok" },
  connecting: { label: "Connecting", variant: "warning" },
  reconnecting: { label: "Reconnecting", variant: "warning" },
  disconnected: { label: "Disconnected", variant: "danger" }
};

const healthBadgeMap: Record<HealthStatus, StatusBadge> = {
  healthy: { label: "Healthy", variant: "ok" },
  loading: { label: "Loading", variant: "warning" },
  error: { label: "Error", variant: "danger" }
};

const socketDetailMap: Record<SocketStatus, (socketUrl: string) => string> = {
  connected: (socketUrl) => `Streaming from ${socketUrl}`,
  connecting: (socketUrl) => `Dialing ${socketUrl}`,
  reconnecting: (socketUrl) => `Retrying ${socketUrl}`,
  disconnected: (socketUrl) => `No connection to ${socketUrl}`
};

const benchmarkDetailMap: Record<HealthStatus, string> = {
  healthy: "Benchmark snapshot loaded.",
  loading: "Waiting for benchmark snapshot.",
  error: "Benchmark snapshot unavailable."
};

const simulationDetailMap: Record<HealthStatus, string> = {
  healthy: "Simulation events loaded.",
  loading: "Awaiting simulation events.",
  error: "Simulation events unavailable."
};

export function ConnectionHealthPanel({
  socketStatus,
  socketUrl,
  benchmarkStatus,
  simulationStatus,
  lastEventAt = null,
  benchmarkDetail,
  simulationDetail
}: ConnectionHealthPanelProps) {
  const socketBadge = socketBadgeMap[socketStatus];
  const benchmarkBadge = healthBadgeMap[benchmarkStatus];
  const simulationBadge = healthBadgeMap[simulationStatus];
  const lastEventLabel = lastEventAt === null ? "Awaiting events" : `${lastEventAt}`;

  return (
    <section aria-label="Connection health" style={panelStyle}>
      <header>
        <h3 style={{ margin: 0 }}>Connection health</h3>
        <p style={{ margin: 0, color: "#94a3b8" }}>
          Live endpoint signals for websocket, benchmarks, and simulations.
        </p>
      </header>
      <ul style={listStyle}>
        <li style={itemStyle}>
          <div style={rowStyle}>
            <span style={{ fontWeight: 600 }}>WebSocket</span>
            <Badge variant={socketBadge.variant}>{socketBadge.label}</Badge>
          </div>
          <span style={detailStyle}>{socketDetailMap[socketStatus](socketUrl)}</span>
        </li>
        <li style={itemStyle}>
          <div style={rowStyle}>
            <span style={{ fontWeight: 600 }}>Benchmark snapshot</span>
            <Badge variant={benchmarkBadge.variant}>{benchmarkBadge.label}</Badge>
          </div>
          <span style={detailStyle}>
            {benchmarkDetail ?? benchmarkDetailMap[benchmarkStatus]}
          </span>
        </li>
        <li style={itemStyle}>
          <div style={rowStyle}>
            <span style={{ fontWeight: 600 }}>Simulation events</span>
            <Badge variant={simulationBadge.variant}>{simulationBadge.label}</Badge>
          </div>
          <span style={detailStyle}>
            {simulationDetail ?? simulationDetailMap[simulationStatus]}
          </span>
        </li>
      </ul>
      <div style={detailStyle}>Last event: {lastEventLabel}</div>
    </section>
  );
}
