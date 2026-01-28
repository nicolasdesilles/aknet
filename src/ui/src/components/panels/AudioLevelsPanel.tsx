import { Panel } from "./Panel";

export function AudioLevelsPanel() {
  return (
    <Panel title="Audio Levels">
      <div className="flex items-center justify-center h-full">
        <div className="text-center space-y-2">
          <p className="text-muted-foreground">Audio Levels Monitoring</p>
          <p className="text-sm text-muted-foreground">Coming Soon</p>
        </div>
      </div>
    </Panel>
  );
}
