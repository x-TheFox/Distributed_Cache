import React from "react";
import "@testing-library/jest-dom/vitest";
import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { BenchmarkPanel } from "@/components/metrics/benchmark-panel";

describe("BenchmarkPanel", () => {
  it("renders ops/sec, p99, and metadata", () => {
    render(
      <BenchmarkPanel
        snapshot={{
          opsPerSec: 120000.4,
          p99Ms: 2.5,
          metadata: {
            timestamp: "2026-04-18T08:25:26Z",
            readOps: 16000
          }
        }}
      />
    );

    expect(screen.getByText(/120000 ops\/sec/i)).toBeInTheDocument();
    expect(screen.getByText(/2\.5 ms/i)).toBeInTheDocument();
    expect(screen.getByText("Timestamp")).toBeInTheDocument();
    expect(screen.getByText("2026-04-18T08:25:26Z")).toBeInTheDocument();
    expect(screen.getByText("Read Ops")).toBeInTheDocument();
    expect(screen.getByText("16000")).toBeInTheDocument();
  });
});
