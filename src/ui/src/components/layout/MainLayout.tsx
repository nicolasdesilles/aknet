import { Sidebar } from "./Sidebar";
import { useNavigation } from "@/hooks/useNavigation";
import { AudioLevelsPanel } from "@/components/panels/AudioLevelsPanel";
import { NetworkingPanel } from "@/components/panels/NetworkingPanel";
import { SettingsPanel } from "@/components/panels/SettingsPanel";
import { StatusBar } from "@/components/statusbar/StatusBar";

export function MainLayout() {
  const { activePanel, navigateTo } = useNavigation();

  // Render the active panel
  const renderPanel = () => {
    switch (activePanel) {
      case "audio-levels":
        return <AudioLevelsPanel />;
      case "networking":
        return <NetworkingPanel />;
      case "settings":
        return <SettingsPanel />;
      default:
        return <AudioLevelsPanel />;
    }
  };

  return (
    <div className="flex h-screen flex-col overflow-hidden">
      <div className="flex flex-1 overflow-hidden">
        <Sidebar activePanel={activePanel} onNavigate={navigateTo} />
        <main className="flex-1 overflow-hidden bg-background">
          {renderPanel()}
        </main>
      </div>
      <StatusBar />
    </div>
  );
}
