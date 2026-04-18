import { listScenarioDefinitions } from "@/lib/simulations/scenario-registry";

export const dynamic = "force-dynamic";
export const fetchCache = "force-no-store";

const noStoreHeaders = { "Cache-Control": "no-store" };

export async function GET() {
  return Response.json({ scenarios: listScenarioDefinitions() }, { headers: noStoreHeaders });
}
