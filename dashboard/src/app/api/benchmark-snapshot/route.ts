import { readFile } from "node:fs/promises";
import path from "node:path";

type RawBenchmark = Record<string, unknown>;

const resolveBenchmarkPath = () =>
  process.env.BENCHMARK_ARTIFACT_PATH ??
  path.resolve(process.cwd(), "..", "bench", "out", "latest.json");

const toCamelCase = (value: string) =>
  value.replace(/_([a-z])/g, (_, letter: string) => letter.toUpperCase());

const normalizeBenchmark = (raw: RawBenchmark) => {
  const opsPerSec = Number(raw.ops_per_sec ?? raw.opsPerSec);
  const p99Ms = Number(raw.p99_ms ?? raw.p99Ms);

  if (!Number.isFinite(opsPerSec) || !Number.isFinite(p99Ms)) {
    throw new Error("missing_fields");
  }

  const metadata: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(raw)) {
    if (key === "ops_per_sec" || key === "opsPerSec" || key === "p99_ms" || key === "p99Ms") {
      continue;
    }
    metadata[toCamelCase(key)] = value;
  }

  if (Object.keys(metadata).length === 0) {
    return { opsPerSec, p99Ms };
  }

  return { opsPerSec, p99Ms, metadata };
};

const isRecord = (value: unknown): value is RawBenchmark =>
  typeof value === "object" && value !== null && !Array.isArray(value);

export async function GET() {
  const artifactPath = resolveBenchmarkPath();

  try {
    const contents = await readFile(artifactPath, "utf8");
    const parsed = JSON.parse(contents) as unknown;

    if (!isRecord(parsed)) {
      return Response.json(
        { message: "Benchmark artifact has an unexpected shape." },
        { status: 500 }
      );
    }

    try {
      const normalized = normalizeBenchmark(parsed);
      return Response.json(normalized);
    } catch (error) {
      if (error instanceof Error && error.message === "missing_fields") {
        return Response.json(
          { message: "Benchmark artifact is missing required fields." },
          { status: 500 }
        );
      }
      throw error;
    }
  } catch (error) {
    const message =
      error instanceof Error && "code" in error && (error as NodeJS.ErrnoException).code === "ENOENT"
        ? "Benchmark artifact not found."
        : "Unable to read benchmark artifact.";
    const status =
      error instanceof Error && "code" in error && (error as NodeJS.ErrnoException).code === "ENOENT"
        ? 404
        : 500;
    return Response.json({ message }, { status });
  }
}
