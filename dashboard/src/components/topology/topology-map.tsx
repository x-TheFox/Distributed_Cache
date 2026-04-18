import React from "react";

export type NodeState = {
  id: string;
  alive: boolean;
};

export type ShardState = {
  id: number;
  owner: string;
};

type TopologyMapProps = {
  nodes: NodeState[];
  shards: ShardState[];
};

export function TopologyMap({ nodes, shards }: TopologyMapProps) {
  const orderedNodes = [...nodes].sort((a, b) => a.id.localeCompare(b.id));
  const orderedShards = [...shards].sort((a, b) => a.id - b.id);

  return (
    <section aria-label="Cluster topology">
      <div>
        <h2>Nodes</h2>
        {orderedNodes.length === 0 ? (
          <p>No nodes reported.</p>
        ) : (
          <ul>
            {orderedNodes.map((node) => (
              <li key={node.id}>
                <div>{node.id}</div>
                <div>{node.alive ? "Alive" : "Down"}</div>
              </li>
            ))}
          </ul>
        )}
      </div>
      <div>
        <h2>Shards</h2>
        {orderedShards.length === 0 ? (
          <p>No shard ownership data.</p>
        ) : (
          <ul>
            {orderedShards.map((shard) => (
              <li key={shard.id}>
                <div>Shard {shard.id}</div>
                <div>Owner: {shard.owner}</div>
              </li>
            ))}
          </ul>
        )}
      </div>
    </section>
  );
}
