import { useEffect, useRef, useState } from "react";
import { AppState } from "@/types/startup";
import { type AppScreen } from "@/types/navigation";
import { useStartupEvents } from "./useStartupEvents";

const STARTUP_SUCCESS_DELAY_MS = 1500;
const STARTUP_FADE_OUT_MS = 300;
const FADE_OUT_START_MS = Math.max(
  STARTUP_SUCCESS_DELAY_MS - STARTUP_FADE_OUT_MS,
  0,
);

export function useAppScreen() {
  const { appState, isComplete, error } = useStartupEvents();

  const isReady = appState === AppState.Active && isComplete && !error;

  const [screen, setScreen] = useState<AppScreen>("startup");
  const [isFadingOut, setIsFadingOut] = useState(false);
  const fadeTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const switchTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    if (!isReady) {
      if (fadeTimeoutRef.current) {
        clearTimeout(fadeTimeoutRef.current);
        fadeTimeoutRef.current = null;
      }
      if (switchTimeoutRef.current) {
        clearTimeout(switchTimeoutRef.current);
        switchTimeoutRef.current = null;
      }
      setTimeout(() => {
        setIsFadingOut(false);
        setScreen("startup");
      }, 0);
      return;
    }

    fadeTimeoutRef.current = setTimeout(() => {
      setIsFadingOut(true);
    }, FADE_OUT_START_MS);

    switchTimeoutRef.current = setTimeout(() => {
      setScreen("main");
    }, STARTUP_SUCCESS_DELAY_MS);

    return () => {
      if (fadeTimeoutRef.current) {
        clearTimeout(fadeTimeoutRef.current);
        fadeTimeoutRef.current = null;
      }
      if (switchTimeoutRef.current) {
        clearTimeout(switchTimeoutRef.current);
        switchTimeoutRef.current = null;
      }
    };
  }, [isReady]);

  return { screen, appState, isFadingOut };
}
