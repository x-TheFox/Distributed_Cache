import json
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
BENCH_OUTPUT = ROOT / "bench/out/latest.json"
BENCH_BIN = ROOT / "build/bench/cache_bench"
SCENARIO_MATRIX = ROOT / "bench/scenarios/scenario_matrix.json"


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


def test_benchmark_json_contains_required_fields():
    data = _load_output()
    for field in ("ops_per_sec", "p99_ms"):
        assert field in data


def test_benchmark_scenario_entries_contain_required_fields():
    data = _load_output()
    required = {
        "name",
        "ops_per_sec",
        "p50_ms",
        "p99_ms",
        "error_rate",
        "coalescing_hit_ratio",
        "status",
    }
    for scenario in data["scenarios"]:
        missing = required - scenario.keys()
        assert not missing, f"Scenario {scenario.get('name', '<unknown>')} missing {missing}"


def test_benchmark_matrix_output_has_required_scenarios():
    original = SCENARIO_MATRIX.read_text()
    matrix = _load_matrix()
    matrix["scenarios"].append(
        {"name": "matrix_override", "description": "Matrix override scenario"}
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
        {"name": special_name, "description": "Needs escaping in JSON output"}
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
