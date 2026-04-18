export type ClusterEvent =
  | { type: "node_heartbeat"; nodeId: string; ts: number }
  | {
      type: "shard_moved";
      shardId: number;
      from: string;
      to: string;
      ts: number;
    }
  | { type: "replica_lag"; shardId: number; lagMs: number; ts: number };

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);

const isFiniteNumber = (value: unknown): value is number =>
  typeof value === "number" && Number.isFinite(value);

export function parseClusterEvent(input: unknown): ClusterEvent {
  if (!isRecord(input)) {
    throw new Error("Cluster event must be an object.");
  }

  const { type } = input;

  if (type === "node_heartbeat") {
    if (typeof input.nodeId !== "string" || !isFiniteNumber(input.ts)) {
      throw new Error("Invalid node_heartbeat event.");
    }

    return {
      type,
      nodeId: input.nodeId,
      ts: input.ts
    };
  }

  if (type === "shard_moved") {
    if (
      !isFiniteNumber(input.shardId) ||
      typeof input.from !== "string" ||
      typeof input.to !== "string" ||
      !isFiniteNumber(input.ts)
    ) {
      throw new Error("Invalid shard_moved event.");
    }

    return {
      type,
      shardId: input.shardId,
      from: input.from,
      to: input.to,
      ts: input.ts
    };
  }

  if (type === "replica_lag") {
    if (
      !isFiniteNumber(input.shardId) ||
      !isFiniteNumber(input.lagMs) ||
      !isFiniteNumber(input.ts)
    ) {
      throw new Error("Invalid replica_lag event.");
    }

    return {
      type,
      shardId: input.shardId,
      lagMs: input.lagMs,
      ts: input.ts
    };
  }

  throw new Error("Unknown cluster event type.");
}
