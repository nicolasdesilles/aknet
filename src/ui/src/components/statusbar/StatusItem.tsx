import type React from "react";

import { cn } from "@/lib/utils";

interface StatusItemProps {
  label: string;
  children: React.ReactNode;
  className?: string;
  labelClassName?: string;
  contentClassName?: string;
}

export function StatusItem({
  label,
  children,
  className,
  labelClassName,
  contentClassName,
}: StatusItemProps) {
  return (
    <div className={cn("flex items-center gap-2", className)}>
      <span
        className={cn(
          "text-[11px] uppercase tracking-wide text-muted-foreground",
          labelClassName,
        )}
      >
        {label}
      </span>
      <span className={cn("text-xs text-foreground", contentClassName)}>
        {children}
      </span>
    </div>
  );
}
