import { useCallback, useState } from "react";
import { call } from "@saucer-dev/types";

import type { AppSettings, Result, SaveResult } from "@/types/settings";

interface UseSettingsState {
  pending: AppSettings | null;
  hasChanges: boolean;
  isSaving: boolean;
}

export function useSettings() {
  const [pending, setPending] = useState<UseSettingsState["pending"]>(null);
  const [hasChanges, setHasChanges] =
    useState<UseSettingsState["hasChanges"]>(false);
  const [isSaving, setIsSaving] = useState<UseSettingsState["isSaving"]>(false);

  const load = useCallback(async () => {
    const pendingJson: string = await call<string>("get_pending_settings", []);
    setPending(JSON.parse(pendingJson));

    const changes: boolean = await call<boolean>(
      "has_pending_settings_changes",
      [],
    );
    setHasChanges(changes);
  }, []);

  const update = useCallback(
    (
      section: keyof AppSettings,
      key: string,
      value: string | number | boolean,
    ) => {
      setPending((previous) => {
        if (!previous) return previous;
        const updated = {
          ...previous,
          [section]: {
            ...(previous[section] as unknown as Record<
              string,
              string | number | boolean
            >),
            [key]: value,
          },
        };
        return updated;
      });
      setHasChanges(true);
    },
    [],
  );

  const resetPending = useCallback(async () => {
    const result = await call<Result>("reset_pending_settings", []);
    if (!result.ok) {
      throw new Error(result.error);
    }
    await load();
  }, [load]);

  const save = useCallback(async () => {
    if (!pending) return null;

    setIsSaving(true);
    try {
      const stageResult = await call<string>("stage_settings", [
        JSON.stringify(pending),
      ]);
      const parsed = JSON.parse(stageResult) as Result;

      if (!parsed.ok) {
        throw new Error(parsed.error);
      }

      const saveJson = await call<string>("save_settings", []);
      const saveResult: SaveResult = JSON.parse(saveJson);

      if (!saveResult.result.ok) {
        throw new Error(saveResult.result.error);
      }

      await load();
      return saveResult;
    } finally {
      setIsSaving(false);
    }
  }, [load, pending]);

  return {
    pending,
    hasChanges,
    isSaving,
    load,
    update,
    resetPending,
    save,
  };
}
