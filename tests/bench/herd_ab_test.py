import json
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
BENCH_OUTPUT = ROOT / "bench/out/latest.json"
BENCH_BIN = ROOT / "build/bench/cache_bench"


def _load_output(extra_args: list[str] | None = None) -> dict:
    if not BENCH_BIN.exists():
        pytest.fail(
            "Benchmark output missing. Build cache_bench and run it to generate "
            f"{BENCH_OUTPUT}."
        )
    BENCH_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if BENCH_OUTPUT.exists():
        BENCH_OUTPUT.unlink()
    try:
        args = [str(BENCH_BIN), "--out", str(BENCH_OUTPUT)]
        if extra_args:
            args.extend(extra_args)
        subprocess.run(args, check=True, capture_output=True, text=True)
        return json.loads(BENCH_OUTPUT.read_text())
    finally:
        if BENCH_OUTPUT.exists():
            BENCH_OUTPUT.unlink()


def test_coalescing_ab_shows_duplicate_work_reduction() -> None:
    data = _load_output()
    scenarios = {s["name"]: s for s in data["scenarios"]}
    assert "coalescing_off" in scenarios
    assert "coalescing_on" in scenarios
    assert (
        scenarios["coalescing_on"]["duplicate_backend_hits"]
        < scenarios["coalescing_off"]["duplicate_backend_hits"]
    )


def test_duplicate_backend_hits_independent_of_read_ratio() -> None:
    low = _load_output(["--ops", "5000", "--read-ratio", "0.2"])
    high = _load_output(["--ops", "5000", "--read-ratio", "0.9"])
    assert low["read_ops"] != high["read_ops"]
    low_scenarios = {s["name"]: s for s in low["scenarios"]}
    high_scenarios = {s["name"]: s for s in high["scenarios"]}
    for label, scenarios in ("low", low_scenarios), ("high", high_scenarios):
        on_hits = scenarios["coalescing_on"]["duplicate_backend_hits"]
        off_hits = scenarios["coalescing_off"]["duplicate_backend_hits"]
        assert off_hits > 0, f"expected backend duplicates in {label} run"
        assert off_hits / max(on_hits, 1.0) >= 4.0
    for name in ("coalescing_off", "coalescing_on"):
        low_hits = low_scenarios[name]["duplicate_backend_hits"]
        high_hits = high_scenarios[name]["duplicate_backend_hits"]
        drift = abs(low_hits - high_hits)
        allowed = max(2.0, 0.2 * max(low_hits, high_hits))
        assert drift <= allowed
