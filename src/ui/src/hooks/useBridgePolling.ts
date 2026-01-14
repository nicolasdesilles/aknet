import { useEffect } from "react";
import { call } from "@saucer-dev/types";

export function useBridgePolling(
  intervalMs: number = 16,
  enabled: boolean = true
) {
  useEffect(() => {
    if (!enabled) return;

    const interval = setInterval(() => {
      call("process_bridge_queue", []);
    }, intervalMs);

    return () => clearInterval(interval);
  }, [intervalMs, enabled]);
}
