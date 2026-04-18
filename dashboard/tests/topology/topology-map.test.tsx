import React from "react";
import "@testing-library/jest-dom/vitest";
import { render, screen, within } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { TopologyMap } from "@/components/topology/topology-map";
import { ConnectionHealthPanel } from "@/components/health/connection-health-panel";
import { SourceBadge } from "@/components/source/source-badge";

describe("TopologyMap", () => {
  it("renders nodes and shard ownership", () => {
    render(
      <TopologyMap
        nodes={[{ id: "n1", alive: true }]}
        shards={[{ id: 0, owner: "n1" }]}
      />
    );
    expect(screen.getByText("n1")).toBeInTheDocument();
    expect(screen.getByText("Shard 0")).toBeInTheDocument();
  });

  it("renders nodes and shards in sorted order", () => {
    const { container } = render(
      <TopologyMap
        nodes={[
          { id: "node-b", alive: true },
          { id: "node-a", alive: false }
        ]}
        shards={[
          { id: 2, owner: "node-b" },
          { id: 1, owner: "node-a" }
        ]}
      />
    );

    const lists = within(container).getAllByRole("list");
    const nodeItems = within(lists[0]).getAllByRole("listitem");
    expect(nodeItems[0]).toHaveTextContent("node-a");
    expect(nodeItems[1]).toHaveTextContent("node-b");

    const shardItems = within(lists[1]).getAllByRole("listitem");
    expect(shardItems[0]).toHaveTextContent("Shard 1");
    expect(shardItems[1]).toHaveTextContent("Shard 2");
  });
});

describe("SourceBadge", () => {
  it("shows LIVE source badge by default", () => {
    render(<SourceBadge mode="LIVE" />);
    expect(screen.getByText("LIVE")).toBeInTheDocument();
  });
});

describe("ConnectionHealthPanel", () => {
  it("renders connection health diagnostics", () => {
    render(
      <ConnectionHealthPanel
        socketStatus="connected"
        socketUrl="ws://localhost:8080/ws"
        benchmarkStatus="healthy"
        simulationStatus="loading"
        lastEventAt={123456}
      />
    );

    expect(screen.getByText("Connection health")).toBeInTheDocument();
    expect(screen.getByText("WebSocket")).toBeInTheDocument();
    expect(screen.getByText("Connected")).toBeInTheDocument();
    expect(screen.getByText("Benchmark snapshot")).toBeInTheDocument();
    expect(screen.getByText("Healthy")).toBeInTheDocument();
    expect(screen.getByText("Simulation events")).toBeInTheDocument();
    expect(screen.getByText("Loading")).toBeInTheDocument();
    expect(screen.getByText("Last event: 123456")).toBeInTheDocument();
  });
});
