import { Badge } from "@/components/ui/badge";
import type { AppStatus } from "@/types/status";

import { StatusItem } from "../StatusItem";

type AknetStatus = AppStatus | "unknown";

interface AknetStatusItemProps {
  status?: AknetStatus | null;
  className?: string;
}

const statusLabels: Record<AknetStatus, string> = {
  off: "off",
  booting: "booting",
  active: "active",
  shutting_down: "shutting down",
  error: "error",
  unknown: "unknown",
};

const statusVariant: Record<
  AknetStatus,
  "success" | "warning" | "destructive" | "outline"
> = {
  off: "outline",
  booting: "warning",
  active: "success",
  shutting_down: "warning",
  error: "destructive",
  unknown: "outline",
};

export function AknetStatusItem({ status, className }: AknetStatusItemProps) {
  const resolved = status ?? "unknown";

  return (
    <StatusItem label="AKNET" className={className}>
      <Badge variant={statusVariant[resolved]}>{statusLabels[resolved]}</Badge>
    </StatusItem>
  );
}
