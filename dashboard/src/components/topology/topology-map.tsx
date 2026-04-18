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
  const listStyle: React.CSSProperties = {
    listStyle: "none",
    padding: 0,
    margin: 0,
    display: "grid",
    gap: "10px"
  };
  const itemStyle: React.CSSProperties = {
    display: "flex",
    justifyContent: "space-between",
    gap: "12px",
    padding: "10px 12px",
    border: "1px solid #334155",
    borderRadius: "10px",
    background: "#111c34"
  };

  return (
    <section
      aria-label="Cluster topology"
      style={{
        border: "1px solid #334155",
        borderRadius: "16px",
        background: "#0f172a",
        padding: "16px",
        display: "grid",
        gap: "16px",
        gridTemplateColumns: "repeat(auto-fit, minmax(260px, 1fr))"
      }}
    >
      <div>
        <h2 style={{ marginTop: 0 }}>Nodes</h2>
        {orderedNodes.length === 0 ? (
          <p style={{ color: "#94a3b8" }}>No nodes reported.</p>
        ) : (
          <ul style={listStyle}>
            {orderedNodes.map((node) => (
              <li key={node.id} style={itemStyle}>
                <div>{node.id}</div>
                <div style={{ color: node.alive ? "#86efac" : "#fda4af" }}>
                  {node.alive ? "Alive" : "Down"}
                </div>
              </li>
            ))}
          </ul>
        )}
      </div>
      <div>
        <h2 style={{ marginTop: 0 }}>Shards</h2>
        {orderedShards.length === 0 ? (
          <p style={{ color: "#94a3b8" }}>No shard ownership data.</p>
        ) : (
          <ul style={listStyle}>
            {orderedShards.map((shard) => (
              <li key={shard.id} style={itemStyle}>
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
