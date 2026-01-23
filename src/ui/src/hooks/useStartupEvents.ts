import { useEffect, useState } from "react";
import type {
  StartupProgress,
  StateChangedEvent,
  SequenceCompletedEvent,
  ErrorEvent,
} from "@/types/startup";
import { AppState } from "@/types/startup";

export function useStartupEvents() {
  const [progress, setProgress] = useState<StartupProgress | null>(null);
  const [appState, setAppState] = useState<AppState>(AppState.Off);
  const [isComplete, setIsComplete] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const handleProgress = (e: CustomEvent<StartupProgress>) => {
      setProgress(e.detail);
      setAppState(e.detail.state);
    };

    const handleStateChanged = (e: CustomEvent<StateChangedEvent>) => {
      setAppState(e.detail.new_state);
    };

    const handleCompleted = (e: CustomEvent<SequenceCompletedEvent>) => {
      setIsComplete(true);
      if (!e.detail.success && e.detail.error) {
        setError(e.detail.error);
      }
    };

    const handleError = (e: CustomEvent<ErrorEvent>) => {
      setError(e.detail.message);
    };

    window.addEventListener(
      "startup:progress",
      handleProgress as EventListener
    );
    window.addEventListener(
      "startup:state",
      handleStateChanged as EventListener
    );
    window.addEventListener(
      "startup:completed",
      handleCompleted as EventListener
    );
    window.addEventListener("startup:error", handleError as EventListener);

    return () => {
      window.removeEventListener(
        "startup:progress",
        handleProgress as EventListener
      );
      window.removeEventListener(
        "startup:state",
        handleStateChanged as EventListener
      );
      window.removeEventListener(
        "startup:completed",
        handleCompleted as EventListener
      );
      window.removeEventListener("startup:error", handleError as EventListener);
    };
  }, []);

  return { progress, appState, isComplete, error };
}
