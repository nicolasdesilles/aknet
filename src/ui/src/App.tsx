import { exposed } from "@saucer-dev/types";

import { Button } from "@/components/ui/button";

import { StartupStatus } from "@/components/StartupStatus";
import { SettingsPanel } from "@/components/SettingsPanel";

function handleClick() {
  exposed<void, [void]>("log_test_msg")();
}

function App() {
  return (
    <div className="flex min-h-svh flex-col items-center justify-center gap-4 p-4">
      <SettingsPanel />
      <StartupStatus />
      <Button onClick={handleClick}>Test Log Message</Button>
    </div>
  );
}

export default App;
