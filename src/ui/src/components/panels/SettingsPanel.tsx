import { useEffect, useMemo, useState } from "react";
import { toast } from "sonner";

import { Panel } from "./Panel";
import { useSettings } from "@/hooks/useSettings";
import type { SaveResult } from "@/types/settings";
import { VALID_SAMPLE_RATES, VALID_BUFFER_SIZES } from "@/types/settings";
import { formatSampleRate, formatBufferSize } from "@/lib/audio";

import { AudioDeviceSelect } from "@/components/AudioDeviceSelect";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
} from "@/components/ui/alert-dialog";

export function SettingsPanel() {
  const { pending, hasChanges, isSaving, load, update, resetPending, save } =
    useSettings();
  const [resetDialogOpen, setResetDialogOpen] = useState(false);
  const [impactDialogOpen, setImpactDialogOpen] = useState(false);
  const [impactMessage, setImpactMessage] = useState<string | null>(null);

  useEffect(() => {
    void load();
  }, [load]);

  const changeSummary = useMemo(() => {
    if (!pending) return "Loading settings...";
    return hasChanges ? "You have unsaved changes" : "All changes saved";
  }, [pending, hasChanges]);

  const handleSave = async () => {
    try {
      const result = await save();
      if (!result) return;
      handleSaveImpact(result);
    } catch (error) {
      toast.error("Failed to save settings", {
        description: error instanceof Error ? error.message : "Unknown error",
      });
    }
  };

  const handleSaveImpact = (result: SaveResult) => {
    if (result.save_impact.app_restart_required) {
      setImpactMessage(
        "The app needs to be restarted for changes to take effect.",
      );
      setImpactDialogOpen(true);
      return;
    }

    if (result.save_impact.modules_restart_required.length > 0) {
      setImpactMessage(
        `Module restart required: ${result.save_impact.modules_restart_required.join(", ")}`,
      );
      setImpactDialogOpen(true);
    }
  };

  const handleConfirmReset = async () => {
    setResetDialogOpen(false);
    try {
      await resetPending();
    } catch (error) {
      toast.error("Failed to reset changes", {
        description: error instanceof Error ? error.message : "Unknown error",
      });
    }
  };

  return (
    <Panel title="Settings">
      <div className="mx-auto flex w-full max-w-4xl flex-col gap-6">
        <div className="flex flex-wrap items-center justify-between gap-3">
          <div className="space-y-1">
            <p className="text-sm font-medium">Configuration</p>
            <p className="text-sm text-muted-foreground">{changeSummary}</p>
          </div>
          <div className="flex items-center gap-2">
            <Button
              variant="outline"
              onClick={() => setResetDialogOpen(true)}
              disabled={!hasChanges || isSaving}
            >
              Reset Changes
            </Button>
            <Button onClick={handleSave} disabled={!hasChanges || isSaving}>
              {isSaving ? "Saving..." : "Save Settings"}
            </Button>
          </div>
        </div>

        {!pending ? (
          <Card>
            <CardContent className="py-10 text-center text-muted-foreground">
              Loading settings...
            </CardContent>
          </Card>
        ) : (
          <>
            <Card>
              <CardHeader>
                <CardTitle>Audio</CardTitle>
                <CardDescription>
                  Configure audio engine defaults.
                </CardDescription>
              </CardHeader>
              <CardContent className="grid gap-4">
                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">Sample Rate</label>
                  <Select
                    value={pending.audio.sampling_rate.toString()}
                    onValueChange={(value) =>
                      update("audio", "sampling_rate", parseInt(value))
                    }
                  >
                    <SelectTrigger>
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      {VALID_SAMPLE_RATES.map((rate) => (
                        <SelectItem key={rate} value={rate.toString()}>
                          {formatSampleRate(rate)}
                        </SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                </div>

                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">Buffer Size</label>
                  <Select
                    value={pending.audio.buffer_size.toString()}
                    onValueChange={(value) =>
                      update("audio", "buffer_size", parseInt(value))
                    }
                  >
                    <SelectTrigger>
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      {VALID_BUFFER_SIZES.map((size) => (
                        <SelectItem key={size} value={size.toString()}>
                          {formatBufferSize(size)}
                        </SelectItem>
                      ))}
                    </SelectContent>
                  </Select>
                </div>

                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">
                    Number of Channels
                  </label>
                  <Input
                    type="number"
                    value={pending.audio.num_channels}
                    onChange={(e) =>
                      update("audio", "num_channels", parseInt(e.target.value))
                    }
                  />
                </div>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <CardTitle>JACK</CardTitle>
                <CardDescription>
                  Configure JACK integration and audio devices.
                </CardDescription>
              </CardHeader>
              <CardContent className="grid gap-4">
                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">Client Name</label>
                  <Input
                    value={pending.jack.client_name}
                    onChange={(e) =>
                      update("jack", "client_name", e.target.value)
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
                      update("jack", "server_executable_path", e.target.value)
                    }
                  />
                </div>

                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">Input Device</label>
                  <AudioDeviceSelect
                    value={pending.audio.input_device_id}
                    onChange={(value) =>
                      update("audio", "input_device_id", value)
                    }
                    filterType="input"
                  />
                </div>

                <div className="grid gap-1.5">
                  <label className="text-sm font-medium">Output Device</label>
                  <AudioDeviceSelect
                    value={pending.audio.output_device_id}
                    onChange={(value) =>
                      update("audio", "output_device_id", value)
                    }
                    filterType="output"
                  />
                </div>

                <div className="flex items-center gap-2">
                  <input
                    type="checkbox"
                    id="auto-manage-server"
                    checked={pending.jack.auto_manage_server}
                    onChange={(e) =>
                      update("jack", "auto_manage_server", e.target.checked)
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
              </CardContent>
              <CardFooter className="justify-end text-xs text-muted-foreground">
                Changes to some settings may require a restart.
              </CardFooter>
            </Card>
          </>
        )}
      </div>

      <AlertDialog open={resetDialogOpen} onOpenChange={setResetDialogOpen}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Discard unsaved changes?</AlertDialogTitle>
            <AlertDialogDescription>
              This will reset pending settings back to the last saved values.
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancel</AlertDialogCancel>
            <AlertDialogAction onClick={handleConfirmReset}>
              Discard Changes
            </AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>

      <AlertDialog open={impactDialogOpen} onOpenChange={setImpactDialogOpen}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Restart required</AlertDialogTitle>
            <AlertDialogDescription>
              {impactMessage ?? "Restart required to apply these changes."}
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogAction onClick={() => setImpactDialogOpen(false)}>
              OK
            </AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>
    </Panel>
  );
}
