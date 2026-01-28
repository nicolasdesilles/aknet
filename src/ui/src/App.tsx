import { StartupScreen } from "@/components/startup/StartupScreen";
import { AppState } from "@/types/startup";
import { MainLayout } from "@/components/layout/MainLayout";
import { useAppScreen } from "@/hooks/useAppScreen";

function App() {
  const { screen, appState, isFadingOut } = useAppScreen();
  // Show startup screen until app is ready
  if (screen === "startup") {
    return (
      <div className="flex min-h-screen flex-col items-center justify-center gap-6 p-4">
        <div className="flex items-center gap-4 mb-8">
          <h1 className="text-2xl font-bold">aknet</h1>
        </div>
        <StartupScreen fadeOut={isFadingOut} />
      </div>
    );
  }

  // Show main application layout after startup
  return <MainLayout />;
}

export default App;
