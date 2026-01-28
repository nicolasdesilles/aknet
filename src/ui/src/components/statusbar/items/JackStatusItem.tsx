import { Badge } from "@/components/ui/badge";

import { StatusItem } from "../StatusItem";

interface JackStatusItemProps {
  serverRunning?: boolean | null;
  clientConnected?: boolean | null;
  className?: string;
}

export function JackStatusItem({
  serverRunning,
  clientConnected,
  className,
}: JackStatusItemProps) {
  const serverKnown = serverRunning !== null && serverRunning !== undefined;
  const clientKnown = clientConnected !== null && clientConnected !== undefined;

  const serverVariant = serverKnown
    ? serverRunning
      ? "success"
      : "destructive"
    : "outline";
  const clientVariant = clientKnown
    ? clientConnected
      ? "success"
      : "destructive"
    : "outline";

  return (
    <StatusItem label="JACK" className={className}>
      <div className="flex items-center gap-1">
        <Badge variant={serverVariant} className="px-1.5">
          S
        </Badge>
        <Badge variant={clientVariant} className="px-1.5">
          C
        </Badge>
      </div>
    </StatusItem>
  );
}
