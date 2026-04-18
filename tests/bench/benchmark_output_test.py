import json
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
BENCH_OUTPUT = ROOT / "bench/out/latest.json"
BENCH_BIN = ROOT / "build/bench/cache_bench"


def _run_bench() -> bool:
    if not BENCH_BIN.exists():
        return False
    BENCH_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(BENCH_BIN), "--out", str(BENCH_OUTPUT)], check=True)
    return True


def _ensure_output() -> None:
    if BENCH_OUTPUT.exists():
        return
    if _run_bench() and BENCH_OUTPUT.exists():
        return
    pytest.fail(
        "Benchmark output missing. Build cache_bench and run it to generate "
        f"{BENCH_OUTPUT}."
    )


def test_benchmark_json_contains_required_fields():
    _ensure_output()
    data = json.loads(BENCH_OUTPUT.read_text())
    for field in ("ops_per_sec", "p99_ms"):
        assert field in data
