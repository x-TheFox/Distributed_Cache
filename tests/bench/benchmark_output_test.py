import json
import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
BENCH_OUTPUT = ROOT / "bench/out/latest.json"
BENCH_BIN = ROOT / "build/bench/cache_bench"
SCENARIO_MATRIX = ROOT / "bench/scenarios/scenario_matrix.json"
BENCH_WORKFLOW = ROOT / ".github/workflows/benchmarks.yml"
MIN_SCENARIO_REQUESTS = 1_000_000


def _run_bench(check: bool = True) -> subprocess.CompletedProcess[str]:
    if not BENCH_BIN.exists():
        pytest.fail(
            "Benchmark output missing. Build cache_bench and run it to generate "
            f"{BENCH_OUTPUT}."
        )
    BENCH_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if BENCH_OUTPUT.exists():
        BENCH_OUTPUT.unlink()
    return subprocess.run(
        [str(BENCH_BIN), "--out", str(BENCH_OUTPUT)],
        check=check,
        capture_output=True,
        text=True,
    )


def _load_output() -> dict:
    _run_bench(check=True)
    return json.loads(BENCH_OUTPUT.read_text())


def _ensure_output() -> None:
    if not BENCH_OUTPUT.exists():
        _load_output()


def _load_matrix() -> dict:
    return json.loads(SCENARIO_MATRIX.read_text())


def _render_svg(output_path: Path) -> str:
    _ensure_output()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["python3", str(ROOT / "bench/render_svg.py"), str(BENCH_OUTPUT), str(output_path)],
        check=True,
        capture_output=True,
        text=True,
    )
    return output_path.read_text()


def test_benchmark_json_contains_required_fields():
    data = _load_output()
    for field in ("ops_per_sec", "p99_ms"):
        assert field in data


def test_benchmark_scenario_entries_contain_required_fields():
    data = _load_output()
    required = {
        "name",
        "requests",
        "ops_per_sec",
        "p50_ms",
        "p99_ms",
        "error_rate",
        "coalescing_hit_ratio",
        "duplicate_backend_hits",
        "status",
    }
    for scenario in data["scenarios"]:
        missing = required - scenario.keys()
        assert not missing, f"Scenario {scenario.get('name', '<unknown>')} missing {missing}"


def test_benchmark_scenario_requests_meet_minimum_threshold():
    data = _load_output()
    for scenario in data["scenarios"]:
        assert (
            scenario["requests"] >= MIN_SCENARIO_REQUESTS
        ), f"Scenario {scenario.get('name', '<unknown>')} below request threshold"


def test_benchmark_matrix_requires_request_thresholds():
    matrix = _load_matrix()
    for scenario in matrix["scenarios"]:
        assert "requests" in scenario, "Scenario matrix entry missing requests field"
        assert (
            scenario["requests"] >= MIN_SCENARIO_REQUESTS
        ), f"Matrix scenario {scenario.get('name', '<unknown>')} below request threshold"


def test_benchmark_matrix_output_has_required_scenarios():
    original = SCENARIO_MATRIX.read_text()
    matrix = _load_matrix()
    matrix["scenarios"].append(
        {
            "name": "matrix_override",
            "description": "Matrix override scenario",
            "requests": MIN_SCENARIO_REQUESTS,
        }
    )
    SCENARIO_MATRIX.write_text(json.dumps(matrix, indent=2))
    try:
        data = _load_output()
        names = {s["name"] for s in data["scenarios"]}
        required = {s["name"] for s in matrix["scenarios"]}
        assert names == required
    finally:
        SCENARIO_MATRIX.write_text(original)
        if BENCH_OUTPUT.exists():
            BENCH_OUTPUT.unlink()


def test_benchmark_fails_on_missing_matrix_file():
    original = SCENARIO_MATRIX.read_text()
    backup = SCENARIO_MATRIX.with_suffix(".json.bak")
    SCENARIO_MATRIX.rename(backup)
    try:
        result = _run_bench(check=False)
        assert result.returncode != 0
        message = (result.stderr + result.stdout).lower()
        assert "scenario" in message and "matrix" in message
    finally:
        if SCENARIO_MATRIX.exists():
            SCENARIO_MATRIX.unlink()
        backup.rename(SCENARIO_MATRIX)
        if BENCH_OUTPUT.exists():
            BENCH_OUTPUT.unlink()


def test_benchmark_fails_on_invalid_matrix_file():
    original = SCENARIO_MATRIX.read_text()
    SCENARIO_MATRIX.write_text("{ invalid json ")
    try:
        result = _run_bench(check=False)
        assert result.returncode != 0
        message = (result.stderr + result.stdout).lower()
        assert "scenario" in message and "matrix" in message
    finally:
        SCENARIO_MATRIX.write_text(original)
        if BENCH_OUTPUT.exists():
            BENCH_OUTPUT.unlink()


def test_benchmark_fails_on_invalid_escape_in_matrix():
    original = SCENARIO_MATRIX.read_text()
    SCENARIO_MATRIX.write_text('{"scenarios":[{"name":"bad\\q"}]}')
    try:
        result = _run_bench(check=False)
        assert result.returncode != 0
        message = (result.stderr + result.stdout).lower()
        assert "scenario" in message and "matrix" in message
    finally:
        SCENARIO_MATRIX.write_text(original)
        if BENCH_OUTPUT.exists():
            BENCH_OUTPUT.unlink()


def test_benchmark_output_escapes_scenario_names():
    original = SCENARIO_MATRIX.read_text()
    matrix = _load_matrix()
    special_name = 'scenario "escape" \\ newline\n'
    matrix["scenarios"].append(
        {
            "name": special_name,
            "description": "Needs escaping in JSON output",
            "requests": MIN_SCENARIO_REQUESTS,
        }
    )
    SCENARIO_MATRIX.write_text(json.dumps(matrix, indent=2))
    try:
        data = _load_output()
        names = {s["name"] for s in data["scenarios"]}
        assert special_name in names
    finally:
        SCENARIO_MATRIX.write_text(original)
        if BENCH_OUTPUT.exists():
            BENCH_OUTPUT.unlink()


def test_svg_contains_scenario_rows(tmp_path: Path):
    svg = _render_svg(tmp_path / "bench.svg")
    assert "Scenario Scoreboard" in svg
    assert "read_heavy" in svg
    assert "thundering_herd" in svg


def test_benchmark_workflow_contains_nightly_schedule_and_artifacts():
    workflow = BENCH_WORKFLOW.read_text()
    assert 'cron: "0 3 * * *"' in workflow
    assert re.search(
        r"strategy:\n\s+matrix:\n\s+profile:\s+\[nightly\]",
        workflow,
    )
    assert "name: benchmark-artifacts-${{ matrix.profile }}" in workflow
