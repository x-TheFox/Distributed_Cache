import React from "react";
import "@testing-library/jest-dom/vitest";
import { act, cleanup, render, screen } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";

import type { ClusterEvent } from "@/contracts/cluster-events";
import { SimulationTimeline } from "@/components/simulations/simulation-timeline";

describe("SimulationTimeline", () => {
  afterEach(() => {
    cleanup();
    vi.clearAllTimers();
    vi.useRealTimers();
  });

  it("plays events in chronological order", () => {
    vi.useFakeTimers();

    const events: ClusterEvent[] = [
      { type: "node_heartbeat", nodeId: "node-b", ts: 30 },
      { type: "replica_lag", shardId: 1, lagMs: 12, ts: 10 },
      { type: "shard_moved", shardId: 2, from: "node-a", to: "node-c", ts: 20 }
    ];

    render(<SimulationTimeline events={events} />);

    const playButton = screen.getByRole("button", { name: /play/i });
    act(() => {
      playButton.click();
    });

    const details = screen.getByTestId("event-details");
    expect(details).toHaveTextContent("Replica lag");
    expect(details).toHaveTextContent("Timestamp: 10");

    act(() => {
      vi.advanceTimersByTime(1000);
    });

    expect(details).toHaveTextContent("Shard moved");
    expect(details).toHaveTextContent("Timestamp: 20");

    act(() => {
      vi.advanceTimersByTime(1000);
    });

    expect(details).toHaveTextContent("Node heartbeat");
    expect(details).toHaveTextContent("Timestamp: 30");
  });

  it("plays events with equal timestamps in input order", () => {
    vi.useFakeTimers();

    const events: ClusterEvent[] = [
      { type: "shard_moved", shardId: 2, from: "node-a", to: "node-c", ts: 10 },
      { type: "node_heartbeat", nodeId: "node-b", ts: 10 },
      { type: "replica_lag", shardId: 3, lagMs: 15, ts: 20 }
    ];

    render(<SimulationTimeline events={events} />);

    const playButton = screen.getByRole("button", { name: /play/i });
    act(() => {
      playButton.click();
    });

    const details = screen.getByTestId("event-details");
    expect(details).toHaveTextContent("Shard moved");
    expect(details).toHaveTextContent("Timestamp: 10");

    act(() => {
      vi.advanceTimersByTime(1000);
    });

    expect(details).toHaveTextContent("Node heartbeat");
    expect(details).toHaveTextContent("Timestamp: 10");

    act(() => {
      vi.advanceTimersByTime(1000);
    });

    expect(details).toHaveTextContent("Replica lag");
    expect(details).toHaveTextContent("Timestamp: 20");
  });
});
