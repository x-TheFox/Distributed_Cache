import React from "react";
import "@testing-library/jest-dom/vitest";
import { act, cleanup, fireEvent, render, screen } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";

import type { ClusterEvent } from "@/contracts/cluster-events";
import { SimulationTimeline } from "@/components/simulations/simulation-timeline";

describe("SimulationTimeline", () => {
  afterEach(() => {
    cleanup();
    vi.clearAllTimers();
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  const scenarioResponse = {
    scenarios: [
      { id: "thundering_herd", label: "Thundering Herd" },
      { id: "failover", label: "Node Failover" },
      { id: "coalescing_ab", label: "Coalescing A/B", plugin: "herd-lab" }
    ]
  };

  const mockScenarioFetch = () =>
    vi
      .fn()
      .mockResolvedValue({ ok: true, json: async () => scenarioResponse });

  const flushScenarioFetch = async () => {
    await Promise.resolve();
    await Promise.resolve();
  };

  it("plays events in chronological order", async () => {
    vi.useFakeTimers();
    vi.stubGlobal("fetch", mockScenarioFetch());

    const events: ClusterEvent[] = [
      { type: "node_heartbeat", nodeId: "node-b", ts: 30 },
      { type: "replica_lag", shardId: 1, lagMs: 12, ts: 10 },
      { type: "shard_moved", shardId: 2, from: "node-a", to: "node-c", ts: 20 }
    ];

    render(<SimulationTimeline events={events} />);
    await act(async () => {
      await flushScenarioFetch();
    });

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

  it("plays events with equal timestamps in input order", async () => {
    vi.useFakeTimers();
    vi.stubGlobal("fetch", mockScenarioFetch());

    const events: ClusterEvent[] = [
      { type: "shard_moved", shardId: 2, from: "node-a", to: "node-c", ts: 10 },
      { type: "node_heartbeat", nodeId: "node-b", ts: 10 },
      { type: "replica_lag", shardId: 3, lagMs: 15, ts: 20 }
    ];

    render(<SimulationTimeline events={events} />);
    await act(async () => {
      await flushScenarioFetch();
    });

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

  it("surfaces plugin scenarios from the registry", async () => {
    vi.stubGlobal("fetch", mockScenarioFetch());

    const events: ClusterEvent[] = [
      { type: "node_heartbeat", nodeId: "node-b", ts: 5 }
    ];

    render(<SimulationTimeline events={events} />);
    await act(async () => {
      await flushScenarioFetch();
    });

    const select = await screen.findByLabelText(/scenario/i);
    expect(select).toHaveValue("thundering_herd");
    expect(
      screen.getByRole("option", { name: /Coalescing A\/B.*herd-lab/i })
    ).toBeInTheDocument();

    act(() => {
      fireEvent.change(select, { target: { value: "coalescing_ab" } });
    });

    expect(screen.getByText(/Plugin:\s*herd-lab/i)).toBeInTheDocument();
  });
});
