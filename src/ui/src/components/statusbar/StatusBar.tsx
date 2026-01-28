import { AknetStatusItem } from "./items/AknetStatusItem";
import { JackStatusItem } from "./items/JackStatusItem";
import { AudioConfigStatusItem } from "./items/AudioConfigStatusItem";
import { JackCpuLoadStatusItem } from "./items/JackCpuLoadStatusItem";
import { useStatusBarStatus } from "@/hooks/useStatusBarStatus";

export function StatusBar() {
  const { snapshot } = useStatusBarStatus();

  return (
    <div className="flex h-7 items-center border-t border-sidebar-border bg-sidebar px-2 text-xs">
      <div className="flex items-center gap-4">
        <AknetStatusItem status={snapshot?.app.status} />
        <JackStatusItem
          serverRunning={snapshot?.jackServer.running}
          clientConnected={snapshot?.jackClient.connected}
        />
      </div>
      <div className="ml-auto flex items-center gap-4">
        <AudioConfigStatusItem
          channels={snapshot?.jackClient.channels}
          sampleRate={snapshot?.audio.sampleRate}
          bufferSize={snapshot?.audio.bufferSize}
        />
        <JackCpuLoadStatusItem cpuLoad={snapshot?.jackRuntime.cpuLoad} />
      </div>
    </div>
  );
}
