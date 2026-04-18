import React from "react";

type SimulationControlsProps = {
  isPlaying: boolean;
  onPlay: () => void;
  onPause: () => void;
  onReset: () => void;
};

const buttonBaseStyle: React.CSSProperties = {
  borderRadius: "999px",
  border: "1px solid #d1d5db",
  background: "#ffffff",
  padding: "6px 14px",
  fontWeight: 600,
  cursor: "pointer"
};

export function SimulationControls({
  isPlaying,
  onPlay,
  onPause,
  onReset
}: SimulationControlsProps) {
  return (
    <div style={{ display: "flex", gap: "12px", alignItems: "center" }}>
      <button
        type="button"
        onClick={isPlaying ? onPause : onPlay}
        style={buttonBaseStyle}
      >
        {isPlaying ? "Pause" : "Play"}
      </button>
      <button type="button" onClick={onReset} style={buttonBaseStyle}>
        Reset
      </button>
    </div>
  );
}
