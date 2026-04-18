# Mega Refresh Design: README, Benchmark SVG, Live Dashboard, and Simulation Lab

## 1. Problem Statement

The current platform has strong backend primitives, but the portfolio experience and operator UX are not yet convincing enough:

1. README does not fully prove live-system behavior and stress outcomes.
2. Benchmark SVG is too shallow (single summary) for scenario-level storytelling.
3. Dashboard visuals and structure are under-polished; user trust in live integration is weak.
4. There is no one-command developer boot workflow that builds and launches the full stack.
5. Simulation and large-scale validation need to explicitly prove herd mitigation and failure behavior.

This mega-spec upgrades all of the above in one coordinated release, while preserving the existing core cache architecture.

## 2. Goals

1. Deliver a dark-mode, shadcn-based dashboard that defaults to live cache integration.
2. Expand benchmarks to a scenario matrix and generate richer SVG summary artifacts.
3. Provide explicit proof of thundering-herd mitigation (A/B coalescing evidence).
4. Add one-command `dev-up.sh` orchestration that builds-if-needed and launches cache + dashboard.
5. Replace reference-only UI artifacts (`dashboard/image`) with real generated screenshot evidence in docs/README.

## 3. Non-Goals

1. Re-architecting core replication/consensus primitives beyond integration touchpoints.
2. Shipping every possible simulation scenario in first pass; instead ship baseline + plugin framework.
3. Replacing existing build system (CMake + existing dashboard toolchain).

## 4. Current State Snapshot

### [DONE]

1. Core cache modules: storage, eviction, RESP, gRPC, hashing, replication, control-plane basics.
2. Existing benchmark harness and CI workflow producing benchmark artifacts.
3. Initial dashboard skeleton with topology/metrics/simulation components.

### [IN-PROGRESS]

1. End-to-end portfolio-level integration polish and proof surfaces.

### [PLANNED]

1. Multi-scenario benchmark expansion + richer SVG schema.
2. Full dashboard UX rebuild on shadcn with dark mode default.
3. Verified live integration badges and health diagnostics.
4. Simulation plugin framework and stress scenario coverage.
5. One-command orchestration for local full-stack execution.

## 5. Architecture (Mega-Spec Lanes)

The release is delivered as one mega-spec with five coordinated lanes:

1. **System Truth Lane**
   - Dashboard defaults to live cache endpoints.
   - Mock mode available only via explicit flag.
   - Persistent source badge (`LIVE`/`MOCK`) and connection-state panel.

2. **Platform Lane**
   - `scripts/dev-up.sh`:
     - detects/builds cache binaries if missing or stale,
     - installs dashboard deps when needed,
     - starts cache server + dashboard,
     - validates service readiness and prints URLs.
   - Supports virtual-node and port overrides.

3. **Benchmark Lane**
   - Scenario matrix benchmark runner.
   - Nightly CI summary generation.
   - SVG upgraded from single metric to scenario scoreboard.

4. **Dashboard Lane**
   - shadcn/ui component system.
   - dark mode default theme.
   - Layout A operator console (simultaneous signal visibility).

5. **Simulation/Test Lane**
   - baseline scenario suite + plugin registration model.
   - stress evidence and replay artifacts integrated into dashboard.

## 6. Dashboard UX Design (Layout A Approved)

### 6.1 Information Architecture

1. **Header**
   - cluster identity
   - environment label
   - live/mock source badge
   - last event timestamp

2. **Performance row**
   - ops/sec, p50, p99, replica lag, herd-coalescing ratio
   - threshold badges (normal/warn/critical)

3. **Topology row**
   - node/shard ownership map
   - rebalance/failover timeline strip

4. **Simulation + replay row**
   - scenario selector
   - playback controls
   - event detail stream

5. **Benchmark snapshot panel**
   - latest scenario table
   - pass/fail indicators
   - delta vs previous nightly baseline

### 6.2 UI Stack

1. shadcn cards, tabs, badge, table, dialog, tooltip, command palette.
2. Dark mode default; light mode optional toggle.
3. No blank “white screen” state: each panel must include explicit empty/loading/error UX.

## 7. Benchmark Expansion and SVG Design

### 7.1 Required Scenario Matrix

All scenarios run with **1M requests per scenario locally**; nightly CI runs full matrix and publishes summary:

1. Read-heavy (95/5)
2. Write-heavy
3. Mixed baseline
4. Hotspot key churn
5. Shard rebalance disturbance
6. Node failover path
7. Thundering herd stress
8. Coalescing A/B (OFF vs ON)
9. Plugin scenarios discovered by scenario registry

### 7.2 SVG Schema Upgrade

Generated SVG must include:

1. per-scenario ops/sec
2. p50/p95/p99 latency
3. error rate
4. coalescing hit ratio (where applicable)
5. status badge (`PASS`/`WARN`/`FAIL`) by policy thresholds
6. generation timestamp + git SHA

## 8. Thundering Herd Proof Contract

For herd mitigation confirmation, every run produces paired evidence:

1. **A/B test**
   - coalescing disabled
   - coalescing enabled

2. **Required deltas**
   - duplicate backend work reduction
   - p99 latency reduction or bounded impact
   - request collapse ratio

3. **Presentation**
   - benchmark JSON + SVG summary
   - dashboard replay panel showing before/after timeline
   - README section with concise interpretation

## 9. Live Integration Contract

Dashboard is considered “live-integrated” only when all conditions hold:

1. WebSocket event stream consumes real cache events by default.
2. Endpoint health checks succeed and are visible in UI.
3. Source badge is `LIVE`.
4. At least one dynamic metric and one topology update are observed from runtime events in e2e validation.

Mock mode may be used only with explicit opt-in flag and must visibly flip source badge to `MOCK`.

## 10. One-Command Developer Workflow

`scripts/dev-up.sh` behavior:

1. Validate required tools.
2. Build cache server if binary missing/stale.
3. Install dashboard dependencies if absent.
4. Start cache server with configured virtual-node and port values.
5. Start dashboard with env wiring to cache endpoints.
6. Perform readiness checks (cache listener, metrics endpoint, dashboard HTTP).
7. Print URLs and process IDs.
8. Provide paired `dev-down.sh` for controlled teardown.

## 11. Testing Strategy

### 11.1 Automated Coverage Requirements

1. C++ core regression tests remain green.
2. Dashboard unit/integration tests cover:
   - live-source badges
   - benchmark snapshot rendering
   - simulation replay ordering
   - error/empty states
3. Dashboard e2e validates websocket-driven value change (not static headings).
4. Benchmark tests validate scenario-output schema and threshold policy.

### 11.2 Stress Validation Requirements

1. Local: 1M request scenarios.
2. Nightly CI: full matrix summary artifact + updated SVG.
3. Optional manual runs may exceed 1M for exploratory stress.

## 12. Documentation and README Refresh

README will be updated to include:

1. architecture/dataflow diagram
2. scenario matrix table
3. latest SVG snapshot and interpretation guide
4. live integration proof checklist
5. herd mitigation proof summary
6. real dashboard screenshots generated from current runtime

`dashboard/image` reference-only folder will be removed and replaced by real generated screenshots in documented artifact location.

## 13. Risks and Mitigations

1. **Mega-spec complexity**
   - Mitigation: strict lane boundaries, milestone gating, and acceptance checks per lane.

2. **UI polish outrunning data truth**
   - Mitigation: enforce live integration contract before final visual polish acceptance.

3. **Scenario explosion**
   - Mitigation: baseline mandatory scenarios + plugin framework for extensibility.

4. **Flaky stress/e2e runs**
   - Mitigation: deterministic fixtures, explicit time budgets, and clear retry/diagnostic outputs.

## 14. Internal Milestones (Single Spec, Ordered Delivery)

1. **Milestone A: Integration Truth**
   - live-data default path + source badges + health diagnostics.
2. **Milestone B: UX Rebuild**
   - shadcn dark-mode operator console.
3. **Milestone C: Benchmark + SVG Upgrade**
   - matrix outputs + richer SVG + README proof blocks.
4. **Milestone D: Simulation Lab**
   - baseline scenarios + plugin scenarios + replay evidence.
5. **Milestone E: Dev Ergonomics**
   - dev-up/dev-down automation and launch docs.

## 15. Acceptance Criteria

Spec is satisfied only if all are true:

1. Dashboard live integration is proven by automated e2e checks and visible UI source indicators.
2. Scenario matrix and richer SVG are produced by CI and reflected in README.
3. Herd mitigation evidence includes A/B metrics and replay visualization.
4. One-command local startup and shutdown scripts work with virtual-node options.
5. Legacy `dashboard/image` reference artifacts are removed in favor of real generated screenshots.
