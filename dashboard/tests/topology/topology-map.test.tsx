import React from "react";
import "@testing-library/jest-dom/vitest";
import { render, screen, within } from "@testing-library/react";
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
