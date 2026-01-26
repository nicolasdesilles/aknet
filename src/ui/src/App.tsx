import { StartupStatus } from "@/components/StartupStatus";
import { SettingsDialog } from "@/components/SettingsDialog";
import { useStartupEvents } from "@/hooks/useStartupEvents";
import { AppState } from "@/types/startup";

function App() {
  const { appState } = useStartupEvents();
  const isStartupRunning = appState === AppState.Booting;

  return (
    <div className="flex min-h-svh flex-col items-center justify-center gap-6 p-4">
      <div className="flex items-center gap-4">
        <h1 className="text-2xl font-bold">aknet</h1>
        <SettingsDialog disabled={isStartupRunning} />
      </div>
      <StartupStatus />
    </div>
  );
}

export default App;
