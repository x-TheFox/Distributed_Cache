export type ScenarioDefinition = {
  id: string;
  label: string;
  plugin?: string;
};

export const baselineScenarios: ScenarioDefinition[] = [
  { id: "thundering_herd", label: "Thundering Herd" },
  { id: "failover", label: "Node Failover" },
  { id: "hotspot_churn", label: "Hotspot Churn" },
  { id: "rebalance", label: "Shard Rebalance" }
];

const pluginScenarios: ScenarioDefinition[] = [
  { id: "coalescing_ab", label: "Coalescing A/B", plugin: "herd-lab" }
];

const mergeScenarios = (...lists: ScenarioDefinition[][]) => {
  const seen = new Set<string>();
  const merged: ScenarioDefinition[] = [];
  for (const list of lists) {
    for (const scenario of list) {
      if (seen.has(scenario.id)) {
        continue;
      }
      seen.add(scenario.id);
      merged.push(scenario);
    }
  }
  return merged;
};

export const scenarioCatalog = {
  baseline: baselineScenarios,
  plugins: pluginScenarios,
  scenarios: mergeScenarios(baselineScenarios, pluginScenarios)
};

export const listScenarioDefinitions = () => [...scenarioCatalog.scenarios];
