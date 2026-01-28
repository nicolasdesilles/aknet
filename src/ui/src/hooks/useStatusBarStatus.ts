import { useCallback, useEffect, useState } from "react";
import { call } from "@saucer-dev/types";

import type { StatusSnapshot, StatusSnapshotUpdate } from "@/types/status";

const isFullSnapshot = (
  update: StatusSnapshot | StatusSnapshotUpdate,
): update is StatusSnapshot => {
  return Boolean(
    update.app &&
    update.jackServer &&
    update.jackClient &&
    update.audio &&
    update.jackRuntime,
  );
};

const mergeSnapshot = (
  previous: StatusSnapshot,
  update: StatusSnapshotUpdate,
): StatusSnapshot => {
  return {
    app: { ...previous.app, ...update.app },
    jackServer: { ...previous.jackServer, ...update.jackServer },
    jackClient: { ...previous.jackClient, ...update.jackClient },
    audio: { ...previous.audio, ...update.audio },
    jackRuntime: { ...previous.jackRuntime, ...update.jackRuntime },
  };
};

export function useStatusBarStatus() {
  const [snapshot, setSnapshot] = useState<StatusSnapshot | null>(null);

  const loadSnapshot = useCallback(async () => {
    try {
      const json = await call<string>("get_status_snapshot", []);
      const parsed = JSON.parse(json) as StatusSnapshot;
      setSnapshot(parsed);
    } catch (error) {
      console.error("Failed to load status snapshot", error);
    }
  }, []);

  useEffect(() => {
    void loadSnapshot();
  }, [loadSnapshot]);

  useEffect(() => {
    const handleTick = (
      event: CustomEvent<StatusSnapshotUpdate | StatusSnapshot>,
    ) => {
      setSnapshot((previous) => {
        if (!previous) {
          return isFullSnapshot(event.detail) ? event.detail : previous;
        }
        return mergeSnapshot(previous, event.detail);
      });
    };

    window.addEventListener("status:tick", handleTick as EventListener);
    return () => {
      window.removeEventListener("status:tick", handleTick as EventListener);
    };
  }, []);

  return { snapshot, reload: loadSnapshot };
}
