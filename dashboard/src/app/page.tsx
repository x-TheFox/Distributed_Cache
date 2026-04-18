"use client";

import { useEffect, useState } from "react";

import type { ClusterEvent } from "@/contracts/cluster-events";
import { TopologyMap, type NodeState, type ShardState } from "@/components/topology/topology-map";
import { createClusterSocket } from "@/lib/ws-client";

const defaultSocketUrl = "ws://localhost:8080/ws";

export default function Page() {
  const [nodes, setNodes] = useState<NodeState[]>([]);
  const [shards, setShards] = useState<ShardState[]>([]);

  useEffect(() => {
    const socketUrl =
      process.env.NEXT_PUBLIC_CLUSTER_WS_URL ?? defaultSocketUrl;

    const socket = createClusterSocket(socketUrl, (event: ClusterEvent) => {
      if (event.type === "node_heartbeat") {
        setNodes((prev) => {
          const index = prev.findIndex((node) => node.id === event.nodeId);
          if (index === -1) {
            return [...prev, { id: event.nodeId, alive: true }];
          }

          const next = [...prev];
          next[index] = { ...next[index], alive: true };
          return next;
        });
        return;
      }

      if (event.type === "shard_moved") {
        setShards((prev) => {
          const index = prev.findIndex((shard) => shard.id === event.shardId);
          if (index === -1) {
            return [...prev, { id: event.shardId, owner: event.to }];
          }

          const next = [...prev];
          next[index] = { ...next[index], owner: event.to };
          return next;
        });
      }
    });

    return () => socket.close();
  }, []);

  return (
    <main>
      <h1>Cluster Topology</h1>
      <TopologyMap nodes={nodes} shards={shards} />
    </main>
  );
}
