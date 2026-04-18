import json
from pathlib import Path


def test_coalescing_ab_shows_duplicate_work_reduction() -> None:
    data = json.loads(Path("bench/out/latest.json").read_text())
    before = next(s for s in data["scenarios"] if s["name"] == "coalescing_off")
    after = next(s for s in data["scenarios"] if s["name"] == "coalescing_on")
    assert after["duplicate_backend_hits"] < before["duplicate_backend_hits"]
