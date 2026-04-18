import path from "node:path";

import { describe, expect, it, afterEach } from "vitest";

import { GET } from "@/app/api/benchmark-snapshot/route";

const fixturePath = path.resolve(
  process.cwd(),
  "tests",
  "fixtures",
  "benchmark.json"
);
const missingPath = path.resolve(
  process.cwd(),
  "tests",
  "fixtures",
  "missing.json"
);

const previousPath = process.env.BENCHMARK_ARTIFACT_PATH;

afterEach(() => {
  if (previousPath === undefined) {
    delete process.env.BENCHMARK_ARTIFACT_PATH;
  } else {
    process.env.BENCHMARK_ARTIFACT_PATH = previousPath;
  }
});

describe("benchmark snapshot API", () => {
  it("returns normalized snapshot with metadata", async () => {
    process.env.BENCHMARK_ARTIFACT_PATH = fixturePath;

    const response = await GET();
    expect(response.status).toBe(200);

    const payload = await response.json();
    expect(payload).toEqual(
      expect.objectContaining({
        opsPerSec: 123456.7,
        p99Ms: 2.5
      })
    );
    expect(payload.metadata).toEqual(
      expect.objectContaining({
        timestamp: "2026-04-18T08:25:26Z",
        readOps: 16000,
        writeOps: 4000
      })
    );
  });

  it("returns an error when artifact is missing", async () => {
    process.env.BENCHMARK_ARTIFACT_PATH = missingPath;

    const response = await GET();
    expect(response.status).toBe(404);

    const payload = await response.json();
    expect(payload.message).toMatch(/not found/i);
  });
});
