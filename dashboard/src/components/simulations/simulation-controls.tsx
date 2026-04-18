import React from "react";

import { Button } from "@/components/ui/button";

type SimulationControlsProps = {
  isPlaying: boolean;
  onPlay: () => void;
  onPause: () => void;
  onReset: () => void;
};

export function SimulationControls({
  isPlaying,
  onPlay,
  onPause,
  onReset
}: SimulationControlsProps) {
  return (
    <div style={{ display: "flex", gap: "12px", alignItems: "center" }}>
      <Button onClick={isPlaying ? onPause : onPlay}>
        {isPlaying ? "Pause" : "Play"}
      </Button>
      <Button onClick={onReset}>Reset</Button>
    </div>
  );
}
