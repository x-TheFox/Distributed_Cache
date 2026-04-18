import React from "react";

import { Button } from "@/components/ui/button";
import type { ScenarioDefinition } from "@/lib/simulations/scenario-registry";

type ScenarioStatus = "loading" | "ready" | "error";

type SimulationControlsProps = {
  isPlaying: boolean;
  onPlay: () => void;
  onPause: () => void;
  onReset: () => void;
  scenarios: ScenarioDefinition[];
  activeScenarioId: string | null;
  onScenarioChange: (scenarioId: string) => void;
  scenarioStatus?: ScenarioStatus;
};

const scenarioSelectStyle: React.CSSProperties = {
  padding: "6px 10px",
  borderRadius: "10px",
  border: "1px solid #334155",
  background: "#0f172a",
  color: "#e2e8f0",
  minWidth: "200px"
};

const scenarioMetaStyle: React.CSSProperties = {
  fontSize: "0.75rem",
  color: "#94a3b8"
};

export function SimulationControls({
  isPlaying,
  onPlay,
  onPause,
  onReset,
  scenarios,
  activeScenarioId,
  onScenarioChange,
  scenarioStatus = "ready"
}: SimulationControlsProps) {
  const activeScenario =
    scenarios.find((scenario) => scenario.id === activeScenarioId) ?? null;
  const scenarioLabel = (scenario: ScenarioDefinition) =>
    scenario.plugin ? `${scenario.label} (${scenario.plugin} plugin)` : scenario.label;
  const scenarioMessage =
    scenarioStatus === "loading"
      ? "Loading scenarios..."
      : scenarioStatus === "error"
        ? "Scenario registry unavailable."
        : activeScenario?.plugin
          ? `Plugin: ${activeScenario.plugin}`
          : null;
  const isScenarioDisabled =
    scenarioStatus !== "ready" || scenarios.length === 0;

  return (
    <div
      style={{
        display: "flex",
        gap: "16px",
        alignItems: "center",
        flexWrap: "wrap"
      }}
    >
      <div style={{ display: "flex", gap: "12px", alignItems: "center" }}>
        <Button onClick={isPlaying ? onPause : onPlay}>
          {isPlaying ? "Pause" : "Play"}
        </Button>
        <Button onClick={onReset}>Reset</Button>
      </div>
      <div style={{ display: "flex", flexDirection: "column", gap: "6px" }}>
        <label
          htmlFor="scenario-select"
          style={{ fontSize: "0.8rem", color: "#94a3b8" }}
        >
          Scenario
        </label>
        <select
          id="scenario-select"
          style={scenarioSelectStyle}
          value={activeScenarioId ?? ""}
          onChange={(event) => onScenarioChange(event.target.value)}
          disabled={isScenarioDisabled}
        >
          {scenarios.length === 0 ? (
            <option value="">
              {scenarioStatus === "loading" ? "Loading scenarios..." : "No scenarios available"}
            </option>
          ) : (
            scenarios.map((scenario) => (
              <option key={scenario.id} value={scenario.id}>
                {scenarioLabel(scenario)}
              </option>
            ))
          )}
        </select>
        {scenarioMessage ? (
          <span style={scenarioMetaStyle}>{scenarioMessage}</span>
        ) : null}
      </div>
    </div>
  );
}
