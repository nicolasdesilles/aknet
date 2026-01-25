import { useState, useEffect, useCallback } from "react";
import { call } from "@saucer-dev/types";

import type { AppSettings, SaveResult, Result } from "@/types/settings";

import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";

export function SettingsPanel() {
  const [pending, setPending] = useState<AppSettings | null>(null);
  const [hasChanges, setHasChanges] = useState(false);

  const loadSettings = useCallback(async () => {
    const pendingJson: string = await call<string>("get_pending_settings", []);
    setPending(JSON.parse(pendingJson));

    const changes: boolean = await call<boolean>(
      "has_pending_settings_changes",
      [],
    );
    setHasChanges(changes);
  }, []);

  useEffect(() => {
    // eslint-disable-next-line
    void loadSettings();
  }, [loadSettings]);

  const handleChange = (
    section: keyof AppSettings,
    key: string,
    value: string | number | boolean,
  ) => {
    if (!pending) return;

    const updated = {
      ...pending,
      [section]: {
        ...(pending[section] as unknown as Record<string, string | number | boolean>),
        [key]: value,
      },
    };
    setPending(updated);
    setHasChanges(true);
  };

  const handleSave = async () => {
    if (!pending) return;

    const stageResult = await call<string>("stage_settings", [
      JSON.stringify(pending),
    ]);
    const parsed = JSON.parse(stageResult);

    if (!parsed.ok) {
      alert(`Failed to stage settings: ${parsed.error}`);
      return;
    }

    const saveJson = await call<string>("save_settings", []);
    const saveResult: SaveResult = JSON.parse(saveJson);

    if (!saveResult.result.ok) {
      alert(`Failed to save settings: ${saveResult.result.error}`);
      return;
    }

    if (saveResult.save_impact.app_restart_required) {
      alert("The app needs to be restarted for the changes to take effect.");
    } else if (saveResult.save_impact.modules_restart_required.length > 0) {
      alert(
        `Module restart required: ${saveResult.save_impact.modules_restart_required.join(", ")}`,
      );
    }

    await loadSettings();
  };

  const handleReset = async () => {
    const result = await call<Result>("reset_pending_settings", []);
    if (!result.ok) {
      alert(`Failed to reset pending settings: ${result.error}`);
      return;
    }
    await loadSettings();
  };

  if (!pending) return <div>Loading...</div>;

  return (
    <Card className="w-full max-w-2xl">
      <CardHeader>
        <CardTitle>Settings</CardTitle>
        <CardDescription>
          {hasChanges ? "You have unsaved changes" : "All changes saved"}
        </CardDescription>
      </CardHeader>

      <CardContent className="space-y-6">
        {/* Audio Settings */}
        <div className="space-y-2">
          <h3 className="text-lg font-semibold">Audio</h3>
          <div className="grid gap-2">
            <label>
              Sample Rate (Hz):
              <Input
                type="number"
                value={pending.audio.sampling_rate}
                onChange={(e) =>
                  handleChange(
                    "audio",
                    "sampling_rate",
                    parseInt(e.target.value),
                  )
                }
              />
            </label>
            <label>
              Buffer Size (samples):
              <Input
                type="number"
                value={pending.audio.buffer_size}
                onChange={(e) =>
                  handleChange("audio", "buffer_size", parseInt(e.target.value))
                }
              />
            </label>
            <label>
              Number of Channels:
              <Input
                type="number"
                value={pending.audio.num_channels}
                onChange={(e) =>
                  handleChange(
                    "audio",
                    "num_channels",
                    parseInt(e.target.value),
                  )
                }
              />
            </label>
          </div>
        </div>

        {/* JACK Settings */}
        <div className="space-y-2">
          <h3 className="text-lg font-semibold">JACK</h3>
          <div className="grid gap-2">
            <label>
              Client Name:
              <Input
                value={pending.jack.client_name}
                onChange={(e) =>
                  handleChange("jack", "client_name", e.target.value)
                }
              />
            </label>
            <label>
              Server Executable Path:
              <Input
                value={pending.jack.server_executable_path}
                onChange={(e) =>
                  handleChange("jack", "server_executable_path", e.target.value)
                }
              />
            </label>
            <label>
              <input
                type="checkbox"
                checked={pending.jack.auto_manage_server}
                onChange={(e) =>
                  handleChange("jack", "auto_manage_server", e.target.checked)
                }
              />{" "}
              Auto-manage server
            </label>
          </div>
        </div>

        {/* Actions */}
        <div className="flex gap-2">
          <Button onClick={handleSave} disabled={!hasChanges}>
            Save Settings
          </Button>
          <Button
            onClick={handleReset}
            variant="outline"
            disabled={!hasChanges}
          >
            Reset Changes
          </Button>
        </div>
      </CardContent>
    </Card>
  );
}
