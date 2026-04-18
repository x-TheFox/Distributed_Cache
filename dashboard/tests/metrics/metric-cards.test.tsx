import React from "react";
import "@testing-library/jest-dom/vitest";
import { render, screen, within } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { MetricCards } from "@/components/metrics/metric-cards";

describe("MetricCards", () => {
  const escapeRegExp = (value: string) =>
    value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

  const getCard = (container: HTMLElement, label: string) => {
    const heading = within(container).getByRole("heading", {
      name: new RegExp(`^${escapeRegExp(label)}$`, "i"),
    });
    const card = heading.closest("article");
    if (!card) {
      throw new Error(`Missing card for ${label}`);
    }
    return card;
  };

  it("shows ops/sec and p99", () => {
    const { container } = render(
      <MetricCards opsPerSec={180000} p99Ms={2.3} replicaLagMs={8} />
    );
    expect(within(container).getByText(/180000/)).toBeInTheDocument();
    expect(within(container).getByText(/2.3 ms/)).toBeInTheDocument();
  });

  it("marks metrics ok at thresholds", () => {
    const { container } = render(
      <MetricCards opsPerSec={150000} p99Ms={5} replicaLagMs={20} />
    );

    const throughputCard = getCard(container, "Throughput");
    expect(throughputCard).toHaveAttribute("data-status", "ok");
    expect(within(throughputCard).getByText(/ok/i)).toBeInTheDocument();

    const latencyCard = getCard(container, "p99 latency");
    expect(latencyCard).toHaveAttribute("data-status", "ok");
    expect(within(latencyCard).getByText(/ok/i)).toBeInTheDocument();

    const replicaCard = getCard(container, "Replica lag");
    expect(replicaCard).toHaveAttribute("data-status", "ok");
    expect(within(replicaCard).getByText(/ok/i)).toBeInTheDocument();
  });

  it("marks metrics warning beyond thresholds", () => {
    const { container } = render(
      <MetricCards opsPerSec={149999} p99Ms={5.1} replicaLagMs={21} />
    );

    const throughputCard = getCard(container, "Throughput");
    expect(throughputCard).toHaveAttribute("data-status", "warning");
    expect(within(throughputCard).getByText(/warning/i)).toBeInTheDocument();

    const latencyCard = getCard(container, "p99 latency");
    expect(latencyCard).toHaveAttribute("data-status", "warning");
    expect(within(latencyCard).getByText(/warning/i)).toBeInTheDocument();

    const replicaCard = getCard(container, "Replica lag");
    expect(replicaCard).toHaveAttribute("data-status", "warning");
    expect(within(replicaCard).getByText(/warning/i)).toBeInTheDocument();
  });
});
