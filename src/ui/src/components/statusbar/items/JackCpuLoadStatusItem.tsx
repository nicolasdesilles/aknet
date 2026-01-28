import { StatusItem } from "../StatusItem";

interface JackCpuLoadStatusItemProps {
  cpuLoad?: number | null;
  className?: string;
}

export function JackCpuLoadStatusItem({
  cpuLoad,
  className,
}: JackCpuLoadStatusItemProps) {
  const display =
    typeof cpuLoad === "number" && Number.isFinite(cpuLoad)
      ? `${cpuLoad.toFixed(1)}%`
      : "—";

  return (
    <StatusItem label="jack cpu" className={className}>
      <span className="font-mono text-xs">{display}</span>
    </StatusItem>
  );
}
