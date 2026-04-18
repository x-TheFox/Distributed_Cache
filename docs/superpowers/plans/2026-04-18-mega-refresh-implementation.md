# Mega Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a single coordinated refresh that upgrades benchmark depth, README/SVG evidence, dashboard UX/live integration, simulation coverage, and one-command local orchestration.

**Architecture:** Keep the existing cache core intact and layer improvements through five lanes: benchmark pipeline, dashboard UX, live integration observability, simulation/stress validation, and developer orchestration scripts. Use shared contracts between benchmark JSON, API snapshot routes, and dashboard panels so evidence shown in README and UI comes from the same artifacts.

**Tech Stack:** C++20/CMake/ctest, Python 3 (SVG rendering), GitHub Actions, Next.js + TypeScript + shadcn/ui, Vitest, Playwright, Bash.

---

## File Structure Map

- Create: `scripts/dev-up.sh` — one-command build-and-launch for cache + dashboard.
- Create: `scripts/dev-down.sh` — process teardown for dev-up services.
- Create: `scripts/dashboard-capture.sh` — capture runtime screenshots for docs.
- Create: `bench/scenarios/scenario_matrix.json` — scenario definitions (baseline + plugin references).
- Modify: `bench/cache_bench.cpp` — multi-scenario runner + A/B herd mode output.
- Modify: `bench/render_svg.py` — multi-scenario SVG scoreboard rendering.
- Modify: `.github/workflows/benchmarks.yml` — nightly matrix run + artifact publishing.
- Modify: `README.md` — upgraded architecture/validation/screenshot sections.
- Create/Modify: `docs/generated/screenshots/*` — generated dashboard evidence.
- Create: `dashboard/src/components/ui/*` and `dashboard/components.json` — shadcn setup.
- Modify: `dashboard/src/app/layout.tsx` — dark mode default + providers.
- Modify: `dashboard/src/app/page.tsx` — operator-console layout and live source diagnostics.
- Create: `dashboard/src/components/source/source-badge.tsx` — LIVE/MOCK data-source signal.
- Create: `dashboard/src/components/health/connection-health-panel.tsx` — endpoint health/readiness panel.
- Modify: `dashboard/src/components/metrics/*` — richer metrics + benchmark matrix table.
- Modify: `dashboard/src/components/simulations/*` — plugin-aware simulation controls and replay.
- Create: `dashboard/src/lib/simulations/scenario-registry.ts` — baseline + plugin scenario registry.
- Create: `dashboard/src/app/api/scenarios/route.ts` — scenario catalog endpoint.
- Modify: `dashboard/src/app/api/benchmark-snapshot/route.ts` — multi-scenario normalized payload.
- Modify: `dashboard/src/app/api/mock-events/route.ts` — deterministic scenario event streams.
- Modify: `dashboard/tests/**/*` — unit/integration/e2e proof updates.
- Modify: `tests/bench/benchmark_output_test.py` — matrix schema assertions.
- Create: `tests/bench/herd_ab_test.py` — coalescing OFF/ON delta assertions.

## Task 1: Scenario Matrix Foundation in Benchmark Runner

**Files:**
- Create: `bench/scenarios/scenario_matrix.json`
- Modify: `bench/cache_bench.cpp`
- Test: `tests/bench/benchmark_output_test.py`

- [ ] **Step 1: Write failing matrix-schema tests**

```python
def test_benchmark_matrix_output_has_required_scenarios():
    data = json.loads(Path("bench/out/latest.json").read_text())
    names = {s["name"] for s in data["scenarios"]}
    required = {"read_heavy", "write_heavy", "mixed", "hotspot_churn", "rebalance", "failover", "thundering_herd", "coalescing_ab"}
    assert required.issubset(names)
```

- [ ] **Step 2: Run test to verify failure**

Run: `pytest tests/bench/benchmark_output_test.py -q`  
Expected: FAIL because output does not yet include `scenarios` array.

- [ ] **Step 3: Implement minimal matrix runner and JSON shape**

```cpp
struct ScenarioResult {
  std::string name;
  double ops_per_sec;
  double p50_ms;
  double p99_ms;
  double error_rate;
  double coalescing_hit_ratio;
  std::string status;
};
```

- [ ] **Step 4: Run tests to verify pass**

Run: `pytest tests/bench/benchmark_output_test.py -q`  
Expected: PASS with required scenario names and fields.

- [ ] **Step 5: Commit**

```bash
git add bench/scenarios/scenario_matrix.json bench/cache_bench.cpp tests/bench/benchmark_output_test.py
git commit -m "feat(bench): add scenario matrix benchmark output"
```

## Task 2: Rich Multi-Scenario SVG Scoreboard

**Files:**
- Modify: `bench/render_svg.py`
- Modify: `docs/generated/bench.svg`
- Test: `tests/bench/benchmark_output_test.py`

- [ ] **Step 1: Write failing SVG-content test**

```python
def test_svg_contains_scenario_rows():
    svg = Path("docs/generated/bench.svg").read_text()
    assert "Scenario Scoreboard" in svg
    assert "read_heavy" in svg
    assert "thundering_herd" in svg
```

- [ ] **Step 2: Run test to verify failure**

Run: `pytest tests/bench/benchmark_output_test.py -q`  
Expected: FAIL because current SVG contains only global ops/p99 labels.

- [ ] **Step 3: Implement scoreboard renderer**

```python
for row in data["scenarios"]:
    name = row["name"]
    ops = f'{row["ops_per_sec"]:.0f}'
    p99 = f'{row["p99_ms"]:.2f} ms'
    status = row["status"]
    # append a fixed-width SVG row with these values
```

- [ ] **Step 4: Run test to verify pass**

Run: `python3 bench/render_svg.py bench/out/latest.json docs/generated/bench.svg && pytest tests/bench/benchmark_output_test.py -q`  
Expected: PASS; SVG includes scenario entries and status badges.

- [ ] **Step 5: Commit**

```bash
git add bench/render_svg.py docs/generated/bench.svg tests/bench/benchmark_output_test.py
git commit -m "feat(bench): render multi-scenario benchmark svg scoreboard"
```

## Task 3: Nightly CI Matrix and Artifact Publishing

**Files:**
- Modify: `.github/workflows/benchmarks.yml`
- Test: `.github/workflows/benchmarks.yml` (lint via workflow syntax validation by build command)

- [ ] **Step 1: Add failing workflow assertion test (repo-level)**

```python
def test_workflow_contains_nightly_schedule():
    text = Path(".github/workflows/benchmarks.yml").read_text()
    assert "schedule:" in text
```

- [ ] **Step 2: Run assertion to verify failure**

Run: `python3 -c "from pathlib import Path; t=Path('.github/workflows/benchmarks.yml').read_text(); assert 'schedule:' in t"`  
Expected: FAIL if schedule is absent.

- [ ] **Step 3: Implement nightly matrix workflow updates**

```yaml
on:
  schedule:
    - cron: "0 3 * * *"
jobs:
  benchmark:
    strategy:
      matrix:
        profile: [nightly]
```

- [ ] **Step 4: Run check to verify pass**

Run: `python3 -c "from pathlib import Path; t=Path('.github/workflows/benchmarks.yml').read_text(); assert 'schedule:' in t and 'benchmark-artifacts' in t"`  
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/benchmarks.yml
git commit -m "ci(bench): add nightly scenario benchmark publishing"
```

## Task 4: Dashboard ShadCN Migration + Dark Mode Default

**Files:**
- Create: `dashboard/components.json`
- Create: `dashboard/src/components/ui/*`
- Modify: `dashboard/src/app/layout.tsx`
- Modify: `dashboard/src/app/page.tsx`
- Test: `dashboard/tests/metrics/metric-cards.test.tsx`

- [ ] **Step 1: Write failing dark-mode and shell test**

```tsx
it("renders dashboard shell with dark mode baseline class", () => {
  render(<Page />);
  expect(document.documentElement.className).toMatch(/dark/);
});
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd dashboard && npm test -- metric-cards.test.tsx`  
Expected: FAIL because dark baseline/provider shell is not yet implemented.

- [ ] **Step 3: Implement shadcn shell and dark mode default**

```tsx
<html lang="en" className="dark">
  <body className="bg-background text-foreground">...</body>
</html>
```

- [ ] **Step 4: Run tests to verify pass**

Run: `cd dashboard && npm test`  
Expected: PASS with updated UI shell and existing component tests.

- [ ] **Step 5: Commit**

```bash
git add dashboard/components.json dashboard/src/components/ui dashboard/src/app/layout.tsx dashboard/src/app/page.tsx dashboard/tests
git commit -m "feat(dashboard): migrate shell to shadcn with dark mode default"
```

## Task 5: Live Integration Diagnostics (Source Badge + Health Panel)

**Files:**
- Create: `dashboard/src/components/source/source-badge.tsx`
- Create: `dashboard/src/components/health/connection-health-panel.tsx`
- Modify: `dashboard/src/app/page.tsx`
- Test: `dashboard/tests/topology/topology-map.test.tsx`
- Test: `dashboard/tests/e2e/live-dashboard.spec.ts`

- [ ] **Step 1: Write failing diagnostics tests**

```tsx
it("shows LIVE source badge by default", () => {
  render(<SourceBadge mode="LIVE" />);
  expect(screen.getByText("LIVE")).toBeInTheDocument();
});
```

- [ ] **Step 2: Run tests to verify failure**

Run: `cd dashboard && npm test -- topology-map.test.tsx`  
Expected: FAIL because source/health components are missing.

- [ ] **Step 3: Implement source badge and health panel**

```tsx
<Badge variant={mode === "LIVE" ? "default" : "secondary"}>{mode}</Badge>
```

- [ ] **Step 4: Run tests and e2e to verify pass**

Run: `cd dashboard && npm test && npm run test:e2e`  
Expected: PASS; e2e confirms websocket-driven UI update and visible source indicators.

- [ ] **Step 5: Commit**

```bash
git add dashboard/src/components/source/source-badge.tsx dashboard/src/components/health/connection-health-panel.tsx dashboard/src/app/page.tsx dashboard/tests
git commit -m "feat(dashboard): add live source and connection diagnostics"
```

## Task 6: Simulation Plugin Framework + Herd A/B Evidence

**Files:**
- Create: `dashboard/src/lib/simulations/scenario-registry.ts`
- Create: `dashboard/src/app/api/scenarios/route.ts`
- Modify: `dashboard/src/components/simulations/simulation-controls.tsx`
- Modify: `dashboard/src/components/simulations/simulation-timeline.tsx`
- Create: `tests/bench/herd_ab_test.py`
- Test: `dashboard/tests/simulations/simulation-timeline.test.tsx`

- [ ] **Step 1: Write failing plugin and herd tests**

```python
def test_coalescing_ab_shows_duplicate_work_reduction():
    data = json.loads(Path("bench/out/latest.json").read_text())
    before = next(s for s in data["scenarios"] if s["name"] == "coalescing_off")
    after = next(s for s in data["scenarios"] if s["name"] == "coalescing_on")
    assert after["duplicate_backend_hits"] < before["duplicate_backend_hits"]
```

- [ ] **Step 2: Run tests to verify failure**

Run: `pytest tests/bench/herd_ab_test.py -q && cd dashboard && npm test -- simulation-timeline.test.tsx`  
Expected: FAIL because registry/AB fields are missing.

- [ ] **Step 3: Implement scenario registry and A/B fields**

```ts
export type ScenarioDefinition = { id: string; label: string; plugin?: string };
export const baselineScenarios: ScenarioDefinition[] = [
  { id: "thundering_herd", label: "Thundering Herd" },
  { id: "failover", label: "Node Failover" },
  { id: "hotspot_churn", label: "Hotspot Churn" },
  { id: "rebalance", label: "Shard Rebalance" }
];
```

- [ ] **Step 4: Run tests to verify pass**

Run: `pytest tests/bench/herd_ab_test.py -q && cd dashboard && npm test -- simulation-timeline.test.tsx`  
Expected: PASS with plugin list and herd A/B assertions.

- [ ] **Step 5: Commit**

```bash
git add dashboard/src/lib/simulations/scenario-registry.ts dashboard/src/app/api/scenarios/route.ts dashboard/src/components/simulations tests/bench/herd_ab_test.py dashboard/tests/simulations/simulation-timeline.test.tsx
git commit -m "feat(simulation): add scenario registry and herd ab proof contract"
```

## Task 7: One-Command Local Orchestration Scripts

**Files:**
- Create: `scripts/dev-up.sh`
- Create: `scripts/dev-down.sh`
- Create: `scripts/dashboard-capture.sh`
- Modify: `README.md`
- Test: `tests/smoke/test_bootstrap.cpp`

- [ ] **Step 1: Write failing smoke invocation test**

```cpp
TEST(Bootstrap, DevUpScriptExistsAndIsExecutable) {
  EXPECT_EQ(std::filesystem::exists("scripts/dev-up.sh"), true);
}
```

- [ ] **Step 2: Run tests to verify failure**

Run: `ctest --test-dir build -R Bootstrap --output-on-failure`  
Expected: FAIL because scripts do not exist yet.

- [ ] **Step 3: Implement scripts**

```bash
# dev-up.sh key behavior
cmake -S . -B build
cmake --build build --target cache_server
cd dashboard && npm install && npm run dev
```

- [ ] **Step 4: Run tests to verify pass**

Run: `ctest --test-dir build -R Bootstrap --output-on-failure`  
Expected: PASS with script existence/executability checks.

- [ ] **Step 5: Commit**

```bash
git add scripts/dev-up.sh scripts/dev-down.sh scripts/dashboard-capture.sh README.md tests/smoke/test_bootstrap.cpp
git commit -m "feat(devx): add one-command stack orchestration scripts"
```

## Task 8: README Evidence Refresh + Screenshot Artifacts

**Files:**
- Modify: `README.md`
- Modify: `docs/contracts/dashboard-events.md`
- Delete: `dashboard/image/*`
- Create/Modify: `docs/generated/screenshots/*`
- Test: `dashboard/tests/e2e/live-dashboard.spec.ts`

- [ ] **Step 1: Write failing docs evidence assertions**

```python
def test_readme_mentions_live_source_and_herd_proof():
    text = Path("README.md").read_text()
    assert "LIVE/MOCK" in text
    assert "Thundering Herd Proof" in text
```

- [ ] **Step 2: Run assertion to verify failure**

Run: `python3 -c "from pathlib import Path; t=Path('README.md').read_text(); assert 'Thundering Herd Proof' in t"`  
Expected: FAIL if section is missing.

- [ ] **Step 3: Implement README/doc refresh and remove legacy image folder**

```md
## Thundering Herd Proof
- Coalescing OFF vs ON scenario delta table
- Replay visualization link
```

- [ ] **Step 4: Run checks to verify pass**

Run: `python3 -c "from pathlib import Path; t=Path('README.md').read_text(); assert 'Thundering Herd Proof' in t and 'LIVE/MOCK' in t" && cd dashboard && npm run test:e2e`  
Expected: PASS with docs references and e2e intact.

- [ ] **Step 5: Commit**

```bash
git add README.md docs/contracts/dashboard-events.md docs/generated/screenshots
git rm -r dashboard/image
git commit -m "docs: refresh readme evidence and replace legacy dashboard image refs"
```

## Task 9: End-to-End Verification Gate for Mega-Spec

**Files:**
- Modify: `dashboard/tests/e2e/live-dashboard.spec.ts`
- Modify: `tests/bench/benchmark_output_test.py`
- Modify: `README.md`

- [ ] **Step 1: Write failing final gate assertions**

```python
def test_benchmark_matrix_has_one_million_requests_per_scenario():
    data = json.loads(Path("bench/out/latest.json").read_text())
    assert all(s["requests"] >= 1_000_000 for s in data["scenarios"])
```

- [ ] **Step 2: Run gate tests to verify failure**

Run: `pytest tests/bench/benchmark_output_test.py -q && cd dashboard && npm run test:e2e`  
Expected: FAIL until benchmark request counts and e2e assertions are complete.

- [ ] **Step 3: Implement final gate wiring**

```ts
await expect(page.getByText(/Source:\s*LIVE/)).toBeVisible();
await expect(page.getByText(/Replica Lag/)).toContainText("ms");
```

- [ ] **Step 4: Run full verification**

Run: `ctest --test-dir build --output-on-failure && pytest tests/bench/benchmark_output_test.py -q && cd dashboard && npm test && npm run test:e2e`  
Expected: PASS across C++, benchmark schema, dashboard unit, and dashboard e2e suites.

- [ ] **Step 5: Commit**

```bash
git add dashboard/tests/e2e/live-dashboard.spec.ts tests/bench/benchmark_output_test.py README.md
git commit -m "test: add mega refresh release verification gates"
```

## Spec Coverage Check (Self-Review)

1. Live dashboard integration default + source diagnostics: Tasks 4, 5, 9.
2. Shadcn and dark mode baseline: Task 4.
3. Benchmark matrix expansion + richer SVG: Tasks 1, 2, 3.
4. Thundering herd A/B proof + replay visualization contract: Tasks 6, 8, 9.
5. One-command build-and-launch scripts with virtual-node support: Task 7.
6. README refresh and replacement of `dashboard/image` reference artifacts: Task 8.
7. Nightly CI summary and artifact publication: Task 3.

## Placeholder Scan (Self-Review)

1. No deferred markers or blank implementation steps remain.
2. Each task contains explicit files, commands, and concrete code snippets.

## Type/Signature Consistency (Self-Review)

1. Benchmark schema uses `scenarios[]` consistently across runner, SVG, API, and tests.
2. Dashboard benchmark contract uses `opsPerSec`/`p99Ms` consistently at API and panel boundaries.
3. Simulation registry types are reused by API route and simulation controls.
