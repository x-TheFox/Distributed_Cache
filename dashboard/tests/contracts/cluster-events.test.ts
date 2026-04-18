import { parseClusterEvent } from "@/contracts/cluster-events";
import { describe, expect, it } from "vitest";

describe("cluster event contract", () => {
  it("parses node_heartbeat event", () => {
    const event = parseClusterEvent({
      type: "node_heartbeat",
      nodeId: "node-a",
      ts: 1700000000
    });
    expect(event.type).toBe("node_heartbeat");
  });
});
