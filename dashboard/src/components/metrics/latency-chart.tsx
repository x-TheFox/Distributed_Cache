import React from "react";

type LatencyChartProps = {
  values: number[];
  width?: number;
  height?: number;
};

export function LatencyChart({ values, width = 320, height = 120 }: LatencyChartProps) {
  const safeValues = values.length > 0 ? values : [0];
  const min = Math.min(...safeValues);
  const max = Math.max(...safeValues);
  const range = max - min || 1;
  const step = safeValues.length > 1 ? width / (safeValues.length - 1) : width;

  const points = safeValues
    .map((value, index) => {
      const x = index * step;
      const y = height - ((value - min) / range) * height;
      return `${x},${y}`;
    })
    .join(" ");

  const lastValue = safeValues[safeValues.length - 1]?.toFixed(2);

  return (
    <figure style={{ margin: 0 }}>
      <svg
        role="img"
        aria-label="Latency trend"
        viewBox={`0 0 ${width} ${height}`}
        width={width}
        height={height}
      >
        <polyline
          points={points}
          fill="none"
          stroke="#2563eb"
          strokeWidth="2"
          strokeLinejoin="round"
          strokeLinecap="round"
        />
        <circle
          cx={(safeValues.length - 1) * step}
          cy={height - ((safeValues[safeValues.length - 1] - min) / range) * height}
          r="3"
          fill="#2563eb"
        />
      </svg>
      <figcaption style={{ fontSize: "0.85rem", color: "#6b7280" }}>
        Latency trend (ms) • latest {lastValue}
      </figcaption>
    </figure>
  );
}
