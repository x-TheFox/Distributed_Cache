import { parseClusterEvent } from "@/contracts/cluster-events";
import { describe, expect, it } from "vitest";

describe("cluster event contract", () => {
  const invalidPayloads: Array<{ label: string; payload: unknown }> = [
    { label: "non-object", payload: null },
    { label: "array payload", payload: [] },
    { label: "unknown type", payload: { type: "not_real", ts: 1 } },
    { label: "missing nodeId", payload: { type: "node_heartbeat", ts: 1 } },
    {
      label: "invalid shard_moved types",
      payload: {
        type: "shard_moved",
        shardId: "nope",
        from: "node-a",
        to: "node-b",
        ts: 1
      }
    },
    {
      label: "invalid replica_lag types",
      payload: {
        type: "replica_lag",
        shardId: 1,
        lagMs: "slow",
        ts: 1
      }
    }
  ];

  it("parses node_heartbeat event", () => {
    const event = parseClusterEvent({
      type: "node_heartbeat",
      nodeId: "node-a",
      ts: 1700000000
    });
    expect(event.type).toBe("node_heartbeat");
  });

  it("parses shard_moved event", () => {
    const event = parseClusterEvent({
      type: "shard_moved",
      shardId: 42,
      from: "node-a",
      to: "node-b",
      ts: 1700000100
    });

    expect(event).toEqual({
      type: "shard_moved",
      shardId: 42,
      from: "node-a",
      to: "node-b",
      ts: 1700000100
    });
  });

  it("parses replica_lag event", () => {
    const event = parseClusterEvent({
      type: "replica_lag",
      shardId: 7,
      lagMs: 1250,
      ts: 1700000200
    });

    expect(event).toEqual({
      type: "replica_lag",
      shardId: 7,
      lagMs: 1250,
      ts: 1700000200
    });
  });

  it.each(invalidPayloads)("rejects $label payload", ({ payload }) => {
    expect(() => parseClusterEvent(payload)).toThrow();
  });
});
