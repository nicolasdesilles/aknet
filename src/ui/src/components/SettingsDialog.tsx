import { useState, useEffect, useCallback } from "react";
import { call } from "@saucer-dev/types";
import { Settings } from "lucide-react";

import type { AppSettings, SaveResult, Result } from "@/types/settings";

import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";

interface SettingsDialogProps {
  disabled?: boolean;
}

export function SettingsDialog({ disabled = false }: SettingsDialogProps) {
  const [open, setOpen] = useState(false);
  const [pending, setPending] = useState<AppSettings | null>(null);
  const [hasChanges, setHasChanges] = useState(false);
  const [isSaving, setIsSaving] = useState(false);

  const loadSettings = useCallback(async () => {
    const pendingJson: string = await call<string>("get_pending_settings", []);
    setPending(JSON.parse(pendingJson));

    const changes: boolean = await call<boolean>(
      "has_pending_settings_changes",
      []
    );
    setHasChanges(changes);
  }, []);

  useEffect(() => {
    if (open) {
      void loadSettings();
    }
  }, [open, loadSettings]);

  const handleChange = (
    section: keyof AppSettings,
    key: string,
    value: string | number | boolean
  ) => {
    if (!pending) return;

    const updated = {
      ...pending,
      [section]: {
        ...(pending[section] as unknown as Record<
          string,
          string | number | boolean
        >),
        [key]: value,
      },
    };
    setPending(updated);
    setHasChanges(true);
  };

  const handleSave = async () => {
    if (!pending) return;

    setIsSaving(true);
    try {
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
          `Module restart required: ${saveResult.save_impact.modules_restart_required.join(", ")}`
        );
      }

      await loadSettings();
      setOpen(false);
    } finally {
      setIsSaving(false);
    }
  };

  const handleReset = async () => {
    const result = await call<Result>("reset_pending_settings", []);
    if (!result.ok) {
      alert(`Failed to reset pending settings: ${result.error}`);
      return;
    }
    await loadSettings();
  };

  const handleOpenChange = (newOpen: boolean) => {
    if (!newOpen && hasChanges) {
      if (
        !confirm(
          "You have unsaved changes. Are you sure you want to close without saving?"
        )
      ) {
        return;
      }
      // Reset changes when closing without saving
      void call<Result>("reset_pending_settings", []);
    }
    setOpen(newOpen);
  };

  return (
    <Dialog open={open} onOpenChange={handleOpenChange}>
      <DialogTrigger asChild>
        <Button variant="outline" size="icon" disabled={disabled}>
          <Settings className="h-4 w-4" />
          <span className="sr-only">Settings</span>
        </Button>
      </DialogTrigger>
      <DialogContent className="max-w-2xl max-h-[85vh] overflow-y-auto">
        <DialogHeader>
          <DialogTitle>Settings</DialogTitle>
          <DialogDescription>
            {hasChanges
              ? "You have unsaved changes"
              : "Configure application settings"}
          </DialogDescription>
        </DialogHeader>

        {!pending ? (
          <div className="py-8 text-center text-muted-foreground">
            Loading settings...
          </div>
        ) : (
          <div className="space-y-6 py-4">
            {/* Audio Settings */}
            <div className="space-y-3">
              <h3 className="text-sm font-semibold uppercase tracking-wide text-muted-foreground">
                Audio
              </h3>
              <div className="grid gap-3">
                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">Sample Rate (Hz)</label>
                  <Input
                    type="number"
                    value={pending.audio.sampling_rate}
                    onChange={(e) =>
                      handleChange(
                        "audio",
                        "sampling_rate",
                        parseInt(e.target.value)
                      )
                    }
                  />
                </div>
                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">
                    Buffer Size (samples)
                  </label>
                  <Input
                    type="number"
                    value={pending.audio.buffer_size}
                    onChange={(e) =>
                      handleChange(
                        "audio",
                        "buffer_size",
                        parseInt(e.target.value)
                      )
                    }
                  />
                </div>
                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">
                    Number of Channels
                  </label>
                  <Input
                    type="number"
                    value={pending.audio.num_channels}
                    onChange={(e) =>
                      handleChange(
                        "audio",
                        "num_channels",
                        parseInt(e.target.value)
                      )
                    }
                  />
                </div>
              </div>
            </div>

            {/* JACK Settings */}
            <div className="space-y-3">
              <h3 className="text-sm font-semibold uppercase tracking-wide text-muted-foreground">
                JACK
              </h3>
              <div className="grid gap-3">
                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">Client Name</label>
                  <Input
                    value={pending.jack.client_name}
                    onChange={(e) =>
                      handleChange("jack", "client_name", e.target.value)
                    }
                  />
                </div>
                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">
                    Server Executable Path
                  </label>
                  <Input
                    value={pending.jack.server_executable_path}
                    onChange={(e) =>
                      handleChange(
                        "jack",
                        "server_executable_path",
                        e.target.value
                      )
                    }
                  />
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="checkbox"
                    id="auto-manage-server"
                    checked={pending.jack.auto_manage_server}
                    onChange={(e) =>
                      handleChange("jack", "auto_manage_server", e.target.checked)
                    }
                    className="h-4 w-4 rounded border-input"
                  />
                  <label
                    htmlFor="auto-manage-server"
                    className="text-sm font-medium"
                  >
                    Auto-manage server
                  </label>
                </div>
              </div>
            </div>
          </div>
        )}

        <DialogFooter>
          <Button
            onClick={handleReset}
            variant="outline"
            disabled={!hasChanges || isSaving}
          >
            Reset Changes
          </Button>
          <Button onClick={handleSave} disabled={!hasChanges || isSaving}>
            {isSaving ? "Saving..." : "Save Settings"}
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
