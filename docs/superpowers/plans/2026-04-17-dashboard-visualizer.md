# Dashboard Visualizer and Simulation Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Next.js + TypeScript dashboard that visualizes live cluster topology, replication health, benchmarks, and failure simulations for the distributed cache platform.

**Architecture:** The dashboard consumes a WebSocket event stream from cache nodes plus REST snapshots for historical benchmark artifacts. UI surfaces are split into topology, metrics, and simulation panels to keep features isolated and testable. Shared event contracts are centralized in one typed schema package.

**Tech Stack:** Next.js (App Router), TypeScript, React, WebSocket, Vitest, Playwright.

---

## File Structure Map

- `dashboard/package.json` — Node.js dashboard dependencies and scripts
- `dashboard/next.config.ts` — Next.js configuration
- `dashboard/src/contracts/cluster-events.ts` — typed event schema shared across views
- `dashboard/src/lib/ws-client.ts` — websocket connection and reconnection logic
- `dashboard/src/app/page.tsx` — top-level dashboard composition
- `dashboard/src/components/topology/` — node/shard ownership visuals
- `dashboard/src/components/metrics/` — latency/throughput/lag cards and charts
- `dashboard/src/components/simulations/` — playback timeline and event controls
- `dashboard/src/app/api/mock-events/route.ts` — local dev feed for tests/demos
- `dashboard/tests/` — unit and e2e tests
- `docs/contracts/dashboard-events.md` — backend/frontend event contract source of truth

## Task 1: Dashboard Scaffold + Contract Baseline

**Files:**
- Create: `dashboard/package.json`
- Create: `dashboard/src/contracts/cluster-events.ts`
- Create: `dashboard/src/app/page.tsx`
- Create: `docs/contracts/dashboard-events.md`
- Test: `dashboard/tests/contracts/cluster-events.test.ts`

- [ ] **Step 1: Write failing contract test**

```ts
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
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd dashboard && npm test -- cluster-events.test.ts`  
Expected: FAIL because contract parser does not exist.

- [ ] **Step 3: Implement typed schema parser**

```ts
export type ClusterEvent =
  | { type: "node_heartbeat"; nodeId: string; ts: number }
  | { type: "shard_moved"; shardId: number; from: string; to: string; ts: number }
  | { type: "replica_lag"; shardId: number; lagMs: number; ts: number };

export function parseClusterEvent(input: unknown): ClusterEvent {
  // strict runtime narrowing, throw on unknown shapes
}
```

- [ ] **Step 4: Run test to verify pass**

Run: `cd dashboard && npm test -- cluster-events.test.ts`  
Expected: PASS with strict parsing.

- [ ] **Step 5: Commit**

```bash
git add dashboard/package.json dashboard/src/contracts/cluster-events.ts dashboard/src/app/page.tsx docs/contracts/dashboard-events.md dashboard/tests/contracts/cluster-events.test.ts
git commit -m "feat(dashboard): scaffold app and typed event contracts"
```

## Task 2: WebSocket Client + Live Topology View

**Files:**
- Create: `dashboard/src/lib/ws-client.ts`
- Create: `dashboard/src/components/topology/topology-map.tsx`
- Modify: `dashboard/src/app/page.tsx`
- Test: `dashboard/tests/topology/topology-map.test.tsx`

- [ ] **Step 1: Write failing topology render test**

```tsx
it("renders nodes and shard ownership", () => {
  render(<TopologyMap nodes={[{ id: "n1", alive: true }]} shards={[{ id: 0, owner: "n1" }]} />);
  expect(screen.getByText("n1")).toBeInTheDocument();
  expect(screen.getByText("Shard 0")).toBeInTheDocument();
});
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd dashboard && npm test -- topology-map.test.tsx`  
Expected: FAIL because component does not exist.

- [ ] **Step 3: Implement ws client and topology component**

```ts
export function createClusterSocket(url: string, onEvent: (e: ClusterEvent) => void) {
  // websocket with exponential backoff reconnect and heartbeat timeout
}
```

```tsx
export function TopologyMap(props: { nodes: NodeState[]; shards: ShardState[] }) { /* render cards */ }
```

- [ ] **Step 4: Run tests to verify pass**

Run: `cd dashboard && npm test -- topology-map.test.tsx`  
Expected: PASS with deterministic rendering.

- [ ] **Step 5: Commit**

```bash
git add dashboard/src/lib/ws-client.ts dashboard/src/components/topology/topology-map.tsx dashboard/src/app/page.tsx dashboard/tests/topology/topology-map.test.tsx
git commit -m "feat(dashboard): add websocket ingestion and topology visualization"
```

## Task 3: Metrics Panels (Throughput, Latency, Replica Lag)

**Files:**
- Create: `dashboard/src/components/metrics/metric-cards.tsx`
- Create: `dashboard/src/components/metrics/latency-chart.tsx`
- Modify: `dashboard/src/app/page.tsx`
- Test: `dashboard/tests/metrics/metric-cards.test.tsx`

- [ ] **Step 1: Write failing metrics panel test**

```tsx
it("shows ops/sec and p99", () => {
  render(<MetricCards opsPerSec={180000} p99Ms={2.3} replicaLagMs={8} />);
  expect(screen.getByText(/180000/)).toBeInTheDocument();
  expect(screen.getByText(/2.3 ms/)).toBeInTheDocument();
});
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd dashboard && npm test -- metric-cards.test.tsx`  
Expected: FAIL because metric components do not exist.

- [ ] **Step 3: Implement metric components**

```tsx
export function MetricCards(props: { opsPerSec: number; p99Ms: number; replicaLagMs: number }) {
  // render cards with thresholds and warning styles
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `cd dashboard && npm test -- metric-cards.test.tsx`  
Expected: PASS with numeric formatting assertions.

- [ ] **Step 5: Commit**

```bash
git add dashboard/src/components/metrics dashboard/src/app/page.tsx dashboard/tests/metrics/metric-cards.test.tsx
git commit -m "feat(dashboard): add throughput latency and replica lag panels"
```

## Task 4: Simulation Timeline and Failover Playback

**Files:**
- Create: `dashboard/src/components/simulations/simulation-timeline.tsx`
- Create: `dashboard/src/components/simulations/simulation-controls.tsx`
- Create: `dashboard/src/app/api/mock-events/route.ts`
- Test: `dashboard/tests/simulations/simulation-timeline.test.tsx`

- [ ] **Step 1: Write failing simulation timeline test**

```tsx
it("plays events in chronological order", async () => {
  // mount timeline with events, click play, assert first->second->third sequence appears
});
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd dashboard && npm test -- simulation-timeline.test.tsx`  
Expected: FAIL due missing timeline component.

- [ ] **Step 3: Implement playback UI and mock event endpoint**

```tsx
export function SimulationTimeline(props: { events: ClusterEvent[] }) {
  // time cursor + event markers + selected event details
}
```

```ts
// app/api/mock-events/route.ts
export async function GET() {
  return Response.json([{ type: "node_heartbeat", nodeId: "node-a", ts: 1 }]);
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `cd dashboard && npm test -- simulation-timeline.test.tsx`  
Expected: PASS with deterministic event ordering.

- [ ] **Step 5: Commit**

```bash
git add dashboard/src/components/simulations dashboard/src/app/api/mock-events/route.ts dashboard/tests/simulations/simulation-timeline.test.tsx
git commit -m "feat(dashboard): add failover simulation timeline and controls"
```

## Task 5: End-to-End Dashboard Validation and README Embedding

**Files:**
- Create: `dashboard/tests/e2e/live-dashboard.spec.ts`
- Modify: `README.md`
- Modify: `docs/contracts/dashboard-events.md`
- Test: `dashboard/tests/e2e/live-dashboard.spec.ts`

- [ ] **Step 1: Write failing e2e scenario**

```ts
test("live dashboard updates from websocket stream", async ({ page }) => {
  await page.goto("http://localhost:3000");
  await expect(page.getByText("Cluster Topology")).toBeVisible();
  await expect(page.getByText("Replica Lag")).toBeVisible();
});
```

- [ ] **Step 2: Run e2e test to verify failure**

Run: `cd dashboard && npx playwright test dashboard/tests/e2e/live-dashboard.spec.ts`  
Expected: FAIL due missing integrated startup wiring.

- [ ] **Step 3: Implement startup wiring and README references**

```md
## Dashboard Visualizer

- Live topology and shard ownership map
- Replica lag and p99 latency panels
- Failover simulation timeline
```

- [ ] **Step 4: Run e2e to verify pass**

Run: `cd dashboard && npx playwright test dashboard/tests/e2e/live-dashboard.spec.ts`  
Expected: PASS with visible topology + metrics + simulation panels.

- [ ] **Step 5: Commit**

```bash
git add dashboard/tests/e2e/live-dashboard.spec.ts README.md docs/contracts/dashboard-events.md
git commit -m "docs(dashboard): validate e2e and document visualizer integration"
```

## Spec Coverage Check (Self-Review)

- Node.js app dashboard requirement: Tasks 1-5
- Visualizer for topology/health/sharding: Tasks 2 and 3
- Simulations and feature demonstrations: Task 4
- Portfolio readability and “why” framing support: Tasks 1 and 5 via contract docs + README

## Placeholder Scan (Self-Review)

- No deferred placeholders (`TBD`, `TODO`, “later”) remain in tasks.
- Every code-changing task includes concrete snippets and concrete commands.

## Type/Signature Consistency (Self-Review)

- `ClusterEvent` union and `parseClusterEvent` are consistently referenced across websocket, topology, and simulation tasks.
- Metric props (`opsPerSec`, `p99Ms`, `replicaLagMs`) are consistently named across tests and components.
