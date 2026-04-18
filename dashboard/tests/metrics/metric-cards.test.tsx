import React from "react";
import "@testing-library/jest-dom/vitest";
import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { MetricCards } from "@/components/metrics/metric-cards";

describe("MetricCards", () => {
  it("shows ops/sec and p99", () => {
    render(<MetricCards opsPerSec={180000} p99Ms={2.3} replicaLagMs={8} />);
    expect(screen.getByText(/180000/)).toBeInTheDocument();
    expect(screen.getByText(/2.3 ms/)).toBeInTheDocument();
  });
});
