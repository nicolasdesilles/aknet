import { formatBufferSize, formatSampleRate } from "@/lib/audio";

interface AudioConfigStatusItemProps {
  channels?: number | null;
  sampleRate?: number | null;
  bufferSize?: number | null;
  className?: string;
}

export function AudioConfigStatusItem({
  channels,
  sampleRate,
  bufferSize,
  className,
}: AudioConfigStatusItemProps) {
  const channelText = channels ?? "—";
  const sampleText = sampleRate ? formatSampleRate(sampleRate) : "—";
  const bufferText = bufferSize ? formatBufferSize(bufferSize) : "—";

  return (
    <div className={className}>
      <span className="font-mono text-xs text-foreground">
        {channelText} ch
      </span>
      <span className="px-1 text-muted-foreground">|</span>
      <span className="font-mono text-xs text-foreground">{sampleText}</span>
      <span className="px-1 text-muted-foreground">|</span>
      <span className="font-mono text-xs text-foreground">{bufferText}</span>
    </div>
  );
}
