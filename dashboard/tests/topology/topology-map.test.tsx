import React from "react";
import "@testing-library/jest-dom/vitest";
import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { TopologyMap } from "@/components/topology/topology-map";

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
});
