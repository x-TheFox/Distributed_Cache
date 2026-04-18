import React from "react";
import type { HTMLAttributes } from "react";

import { cn } from "@/lib/utils";

type BadgeVariant = "ok" | "warning" | "danger";

type BadgeProps = HTMLAttributes<HTMLSpanElement> & {
  variant?: BadgeVariant;
};

const variantClass: Record<BadgeVariant, string> = {
  ok: "ui-badge-ok",
  warning: "ui-badge-warning",
  danger: "ui-badge-danger"
};

export function Badge({
  className,
  variant = "ok",
  ...props
}: BadgeProps) {
  return <span className={cn("ui-badge", variantClass[variant], className)} {...props} />;
}
