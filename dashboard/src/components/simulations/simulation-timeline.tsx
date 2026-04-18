"use client";

import React, { useEffect, useMemo, useState } from "react";

import type { ClusterEvent } from "@/contracts/cluster-events";
import type { ScenarioDefinition } from "@/lib/simulations/scenario-registry";

import { SimulationControls } from "./simulation-controls";

type SimulationTimelineProps = {
  events: ClusterEvent[];
  intervalMs?: number;
};

type OrderedEvent = {
  event: ClusterEvent;
  order: number;
};

type ScenarioStatus = "loading" | "ready" | "error";

const timelineContainerStyle: React.CSSProperties = {
  border: "1px solid #334155",
  borderRadius: "16px",
  padding: "16px",
  background: "#0f172a",
  display: "flex",
  flexDirection: "column",
  gap: "16px"
};

const eventCardStyle: React.CSSProperties = {
  border: "1px solid #334155",
  borderRadius: "12px",
  padding: "12px",
  background: "#111c34"
};

const eventTitle = (event: ClusterEvent) => {
  switch (event.type) {
    case "node_heartbeat":
      return "Node heartbeat";
    case "shard_moved":
      return "Shard moved";
    case "replica_lag":
      return "Replica lag";
    default:
      return "Cluster event";
  }
};

const eventDescription = (event: ClusterEvent) => {
  switch (event.type) {
    case "node_heartbeat":
      return `Node ${event.nodeId} heartbeat received.`;
    case "shard_moved":
      return `Shard ${event.shardId} moved from ${event.from} to ${event.to}.`;
    case "replica_lag":
      return `Shard ${event.shardId} lagged at ${event.lagMs} ms.`;
    default:
      return "Cluster event update.";
  }
};

const sortEvents = (events: ClusterEvent[]): OrderedEvent[] =>
  events
    .map((event, index) => ({ event, order: index }))
    .sort((a, b) => (a.event.ts - b.event.ts) || a.order - b.order);

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);

const isScenarioDefinition = (value: unknown): value is ScenarioDefinition => {
  if (!isRecord(value)) {
    return false;
  }
  if (typeof value.id !== "string" || typeof value.label !== "string") {
    return false;
  }
  if ("plugin" in value && value.plugin !== undefined && typeof value.plugin !== "string") {
    return false;
  }
  return true;
};

const parseScenarioPayload = (payload: unknown): ScenarioDefinition[] | null => {
  if (!isRecord(payload) || !Array.isArray(payload.scenarios)) {
    return null;
  }

  if (!payload.scenarios.every(isScenarioDefinition)) {
    return null;
  }

  return payload.scenarios;
};

export function SimulationTimeline({
  events,
  intervalMs = 1000
}: SimulationTimelineProps) {
  const orderedEvents = useMemo(() => sortEvents(events), [events]);
  const [currentIndex, setCurrentIndex] = useState<number | null>(
    orderedEvents.length > 0 ? 0 : null
  );
  const [isPlaying, setIsPlaying] = useState(false);
  const [scenarios, setScenarios] = useState<ScenarioDefinition[]>([]);
  const [scenarioStatus, setScenarioStatus] = useState<ScenarioStatus>("loading");
  const [activeScenarioId, setActiveScenarioId] = useState<string | null>(null);

  useEffect(() => {
    if (orderedEvents.length === 0) {
      setCurrentIndex(null);
      setIsPlaying(false);
      return;
    }

    setCurrentIndex((prev) => {
      if (prev === null || prev >= orderedEvents.length) {
        return 0;
      }
      return prev;
    });
  }, [orderedEvents.length]);

  useEffect(() => {
    let cancelled = false;
    const controller = new AbortController();

    const loadScenarios = async () => {
      try {
        setScenarioStatus("loading");
        const response = await fetch("/api/scenarios", { signal: controller.signal });
        if (!response.ok) {
          if (!cancelled) {
            setScenarioStatus("error");
          }
          return;
        }

        const payload = await response.json();
        const parsed = parseScenarioPayload(payload);
        if (!parsed) {
          if (!cancelled) {
            setScenarioStatus("error");
          }
          return;
        }

        if (!cancelled) {
          setScenarios(parsed);
          setScenarioStatus("ready");
        }
      } catch {
        if (!cancelled) {
          setScenarioStatus("error");
        }
      }
    };

    loadScenarios();

    return () => {
      cancelled = true;
      controller.abort();
    };
  }, []);

  useEffect(() => {
    if (scenarios.length === 0) {
      setActiveScenarioId(null);
      return;
    }

    setActiveScenarioId((prev) =>
      prev && scenarios.some((scenario) => scenario.id === prev) ? prev : scenarios[0].id
    );
  }, [scenarios]);

  useEffect(() => {
    if (!isPlaying || orderedEvents.length === 0) {
      return undefined;
    }

    const timer = window.setInterval(() => {
      setCurrentIndex((prev) => {
        if (prev === null) {
          return 0;
        }

        const next = prev + 1;
        if (next >= orderedEvents.length) {
          setIsPlaying(false);
          return prev;
        }

        return next;
      });
    }, intervalMs);

    return () => window.clearInterval(timer);
  }, [intervalMs, isPlaying, orderedEvents.length]);

  const activeIndex = currentIndex ?? 0;
  const activeEvent = currentIndex !== null ? orderedEvents[currentIndex]?.event : null;
  const maxIndex = Math.max(orderedEvents.length - 1, 0);

  const handlePlay = () => {
    if (orderedEvents.length === 0) {
      return;
    }
    if (currentIndex === null) {
      setCurrentIndex(0);
    }
    setIsPlaying(true);
  };

  const handlePause = () => setIsPlaying(false);

  const handleReset = () => {
    setIsPlaying(false);
    setCurrentIndex(orderedEvents.length > 0 ? 0 : null);
  };

  const handleScenarioChange = (scenarioId: string) => {
    setActiveScenarioId(scenarioId);
    setIsPlaying(false);
    setCurrentIndex(orderedEvents.length > 0 ? 0 : null);
  };

  const handleScrub = (value: number) => {
    setIsPlaying(false);
    setCurrentIndex(value);
  };

  return (
    <section aria-label="Simulation timeline" style={timelineContainerStyle}>
      <header style={{ display: "flex", justifyContent: "space-between", gap: "16px" }}>
        <div>
          <h3 style={{ margin: 0 }}>Failover playback</h3>
          <p style={{ margin: 0, color: "#94a3b8" }}>
            Step through replay events to understand failover timing.
          </p>
        </div>
        <SimulationControls
          isPlaying={isPlaying}
          onPlay={handlePlay}
          onPause={handlePause}
          onReset={handleReset}
          scenarios={scenarios}
          activeScenarioId={activeScenarioId}
          onScenarioChange={handleScenarioChange}
          scenarioStatus={scenarioStatus}
        />
      </header>

      {orderedEvents.length === 0 ? (
        <p style={{ margin: 0 }}>No simulation events loaded.</p>
      ) : (
        <>
          <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
            <label
              htmlFor="timeline-position"
              style={{ fontSize: "0.85rem", color: "#94a3b8" }}
            >
              Timeline position ({activeIndex + 1} of {orderedEvents.length})
            </label>
            <input
              id="timeline-position"
              type="range"
              min={0}
              max={maxIndex}
              value={activeIndex}
              onChange={(event) => handleScrub(Number(event.target.value))}
              aria-label="Timeline position"
            />
          </div>

          <div data-testid="event-details" aria-live="polite" style={eventCardStyle}>
            {activeEvent ? (
              <>
                <div style={{ fontWeight: 600 }}>{eventTitle(activeEvent)}</div>
                <div style={{ color: "#cbd5e1" }}>{eventDescription(activeEvent)}</div>
                <div style={{ fontSize: "0.8rem", color: "#94a3b8" }}>
                  Timestamp: {activeEvent.ts}
                </div>
              </>
            ) : (
              <div style={{ color: "#94a3b8" }}>No event selected.</div>
            )}
          </div>

          <ol
            style={{
              listStyle: "none",
              padding: 0,
              margin: 0,
              display: "grid",
              gap: "8px"
            }}
          >
            {orderedEvents.map(({ event, order }, index) => {
              const isActive = index === activeIndex;
              return (
                <li
                  key={`${event.type}-${order}`}
                  style={{
                    ...eventCardStyle,
                    borderColor: isActive ? "#38bdf8" : "#334155",
                    background: isActive ? "#0b2945" : "#111c34"
                  }}
                >
                  <div style={{ fontWeight: 600 }}>{eventTitle(event)}</div>
                  <div style={{ color: "#cbd5e1" }}>{eventDescription(event)}</div>
                  <div style={{ fontSize: "0.8rem", color: "#94a3b8" }}>
                    Timestamp: {event.ts}
                  </div>
                </li>
              );
            })}
          </ol>
        </>
      )}
    </section>
  );
}
