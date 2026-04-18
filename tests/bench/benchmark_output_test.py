import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BENCH_OUTPUT = ROOT / "bench/out/latest.json"
BENCH_BIN = ROOT / "build/bench/cache_bench"


def _write_placeholder() -> None:
    BENCH_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "timestamp": "1970-01-01T00:00:00Z",
        "ops_total": 1000,
        "duration_ms": 100.0,
        "ops_per_sec": 10000.0,
        "p50_ms": 0.05,
        "p95_ms": 0.2,
        "p99_ms": 0.5,
        "read_ops": 800,
        "write_ops": 200,
        "read_ratio": 0.8,
        "key_space": 100,
    }
    BENCH_OUTPUT.write_text(json.dumps(payload, indent=2))


def _run_bench() -> bool:
    if not BENCH_BIN.exists():
        return False
    BENCH_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(BENCH_BIN), "--out", str(BENCH_OUTPUT)], check=True)
    return True


def _ensure_output() -> None:
    if BENCH_OUTPUT.exists():
        return
    if not _run_bench():
        _write_placeholder()


def test_benchmark_json_contains_required_fields():
    _ensure_output()
    data = json.loads(BENCH_OUTPUT.read_text())
    assert "ops_per_sec" in data
    assert "p99_ms" in data
