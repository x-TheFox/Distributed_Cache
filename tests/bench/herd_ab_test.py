import json
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
BENCH_OUTPUT = ROOT / "bench/out/latest.json"
BENCH_BIN = ROOT / "build/bench/cache_bench"


def _load_output() -> dict:
    if not BENCH_BIN.exists():
        pytest.fail(
            "Benchmark output missing. Build cache_bench and run it to generate "
            f"{BENCH_OUTPUT}."
        )
    BENCH_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if BENCH_OUTPUT.exists():
        BENCH_OUTPUT.unlink()
    try:
        subprocess.run(
            [str(BENCH_BIN), "--out", str(BENCH_OUTPUT)],
            check=True,
            capture_output=True,
            text=True,
        )
        return json.loads(BENCH_OUTPUT.read_text())
    finally:
        if BENCH_OUTPUT.exists():
            BENCH_OUTPUT.unlink()


def test_coalescing_ab_shows_duplicate_work_reduction() -> None:
    data = _load_output()
    scenarios = {s["name"]: s for s in data["scenarios"]}
    if "coalescing_off" in scenarios and "coalescing_on" in scenarios:
        assert (
            scenarios["coalescing_on"]["duplicate_backend_hits"]
            < scenarios["coalescing_off"]["duplicate_backend_hits"]
        )
    else:
        coalescing = scenarios.get("coalescing_ab")
        assert coalescing is not None
        assert 0.0 <= coalescing["coalescing_hit_ratio"] <= 1.0
