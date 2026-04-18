import React from "react";

import { Badge } from "@/components/ui/badge";

export type SourceMode = "LIVE" | "MOCK";

type SourceBadgeProps = {
  mode: SourceMode;
  className?: string;
};

const variantMap = {
  LIVE: "ok",
  MOCK: "warning"
} as const;

export function SourceBadge({ mode, className }: SourceBadgeProps) {
  return (
    <Badge className={className} variant={variantMap[mode]} data-mode={mode.toLowerCase()}>
      {mode}
    </Badge>
  );
}
