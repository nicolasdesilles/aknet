import { useStartupEvents } from "@/hooks/useStartupEvents";
import { useSettings } from "@/hooks/useSettings";
import { AppState } from "@/types/startup";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Progress } from "@/components/ui/progress";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { toast } from "sonner";

import { Play, Square, RotateCcw } from "lucide-react";

import { call } from "@saucer-dev/types";
import { StepList } from "./StepList";
import { useEffect, useMemo, useRef, useState } from "react";
import { AudioDeviceSelect } from "@/components/AudioDeviceSelect";
import { VALID_SAMPLE_RATES, VALID_BUFFER_SIZES } from "@/types/settings";
import { formatSampleRate, formatBufferSize } from "@/lib/audio";

interface StartupScreenProps {
  fadeOut?: boolean;
}

function getAppStateLabel(state: AppState): string {
  switch (state) {
    case AppState.Off:
      return "Off";
    case AppState.Booting:
      return "Booting";
    case AppState.Active:
      return "Active";
    case AppState.ShuttingDown:
      return "Shutting Down";
    default:
      return "Unknown";
  }
}

function getAppStateBadgeVariant(
  state: AppState,
): "default" | "secondary" | "destructive" | "outline" {
  switch (state) {
    case AppState.Off:
      return "secondary";
    case AppState.Booting:
      return "default";
    case AppState.Active:
      return "outline";
    case AppState.ShuttingDown:
      return "destructive";
    default:
      return "secondary";
  }
}

export function StartupScreen({ fadeOut = false }: StartupScreenProps) {
  const { progress, appState, isComplete, error } = useStartupEvents();
  const { pending, isSaving, load, update, save } = useSettings();

  const [isTransitioning, setIsTransitioning] = useState(false);
  const [startRequested, setStartRequested] = useState(false);
  const startTriggeredRef = useRef(false);
  const transitionTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(
    null,
  );

  const hasSteps = Boolean(progress?.steps?.length);
  const isOff = appState === AppState.Off;
  const shouldShowSettings = isOff && !hasSteps && !startRequested;

  useEffect(() => {
    void load();
  }, [load]);

  const progressPercent = progress
    ? ((progress.current_step_index + 1) / Math.max(progress.steps.length, 1)) *
      100
    : 0;

  const handleStart = async () => {
    try {
      await save();
    } catch (saveError) {
      toast.error("Failed to save startup settings", {
        description:
          saveError instanceof Error ? saveError.message : "Unknown error",
      });
      return;
    }

    setStartRequested(true);
    setIsTransitioning(true);
    requestAnimationFrame(() => setIsTransitioning(false));

    if (transitionTimeoutRef.current) {
      clearTimeout(transitionTimeoutRef.current);
    }

    transitionTimeoutRef.current = setTimeout(async () => {
      if (!startTriggeredRef.current) {
        startTriggeredRef.current = true;
        await call<boolean>("start_startup", []);
      }
    }, 300);
  };

  const handleAbort = async () => {
    await call<void>("abort_startup", []);
  };

  const handleRetry = async () => {
    await call<boolean>("retry_startup", []);
  };

  // Show toast notifications for errors
  useEffect(() => {
    if (error) {
      toast.error("Startup Failed", {
        description: error,
        duration: 5000,
      });
    }
  }, [error]);

  useEffect(() => {
    if (isComplete && !error && appState === AppState.Active) {
      toast.success("Startup Complete", {
        description: "aknet is now active and ready",
        duration: 3000,
      });
    }
  }, [isComplete, error, appState]);

  useEffect(() => {
    return () => {
      if (transitionTimeoutRef.current) {
        clearTimeout(transitionTimeoutRef.current);
        transitionTimeoutRef.current = null;
      }
    };
  }, []);

  const isRunning = appState === AppState.Booting;
  const canRetry = progress?.can_retry && !isRunning;

  const settingsSummary = useMemo(() => {
    if (!pending) return "Loading settings...";
    return "Configure startup audio settings before booting.";
  }, [pending]);

  return (
    <div className={fadeOut ? "transitioning-out" : ""}>
      {shouldShowSettings ? (
        <Card className="w-[520px] flex flex-col">
          <CardHeader className="flex-shrink-0">
            <div className="flex items-center justify-between">
              <CardTitle>Startup Settings</CardTitle>
              <Badge variant={getAppStateBadgeVariant(appState)}>
                {getAppStateLabel(appState)}
              </Badge>
            </div>
            <CardDescription>{settingsSummary}</CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            {!pending ? (
              <div className="py-6 text-center text-muted-foreground">
                Loading settings...
              </div>
            ) : (
              <div className="grid gap-4">
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
              </div>
            )}
          </CardContent>
          <CardContent className="pt-0">
            <Button
              onClick={handleStart}
              className="w-full"
              disabled={!pending || isSaving}
            >
              <Play className="h-4 w-4 mr-2" />
              {isSaving ? "Saving..." : "Start"}
            </Button>
          </CardContent>
        </Card>
      ) : (
        <Card
          className={`w-[450px] h-[400px] flex flex-col transition-all duration-300 ${
            isTransitioning
              ? "opacity-0 translate-y-2"
              : "opacity-100 translate-y-0"
          }`}
        >
          <CardHeader className="flex-shrink-0">
            <div className="flex items-center justify-between">
              <CardTitle>Startup Status</CardTitle>
              <Badge variant={getAppStateBadgeVariant(appState)}>
                {getAppStateLabel(appState)}
              </Badge>
            </div>
            <CardDescription>
              {isComplete
                ? error
                  ? "Startup failed"
                  : "Startup complete"
                : appState === AppState.Off
                  ? "Ready to start"
                  : "Initializing system..."}
            </CardDescription>
          </CardHeader>

          <CardContent className="flex-1 flex flex-col min-h-0 space-y-4">
            {/* Progress bar */}
            {isRunning && (
              <div className="space-y-2 flex-shrink-0">
                <div className="flex justify-between text-sm text-muted-foreground">
                  <span>Progress</span>
                  <span className="font-mono">
                    {Math.round(progressPercent)}%
                  </span>
                </div>
                <Progress
                  value={progressPercent}
                  className="transition-all duration-400"
                />
              </div>
            )}

            {/* Scrollable step list */}
            {progress && progress.steps.length > 0 && (
              <StepList
                steps={progress.steps}
                currentStepIndex={progress.current_step_index}
              />
            )}

            {/* Control buttons */}
            <div className="flex gap-2 flex-shrink-0">
              {isRunning && (
                <Button
                  onClick={handleAbort}
                  variant="destructive"
                  className="flex-1"
                >
                  <Square className="h-4 w-4 mr-2" />
                  Abort
                </Button>
              )}
              {canRetry && (
                <Button
                  onClick={handleRetry}
                  variant="outline"
                  className="flex-1"
                >
                  <RotateCcw className="h-4 w-4 mr-2" />
                  Retry
                </Button>
              )}
            </div>
          </CardContent>
        </Card>
      )}
    </div>
  );
}
