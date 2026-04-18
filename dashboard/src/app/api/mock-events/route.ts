import type { ClusterEvent } from "@/contracts/cluster-events";

const mockEvents: ClusterEvent[] = [
  { type: "node_heartbeat", nodeId: "node-a", ts: 1 },
  { type: "replica_lag", shardId: 2, lagMs: 14, ts: 3 },
  { type: "shard_moved", shardId: 1, from: "node-a", to: "node-b", ts: 5 }
];

export async function GET() {
  return Response.json(mockEvents);
}
