import { useStartupEvents } from "@/hooks/useStartupEvents";
import { AppState, StepStatus } from "@/types/startup";
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
import {
  CheckCircle,
  XCircle,
  Loader2,
  Clock,
  SkipForward,
  AlertTriangle,
  Play,
  Square,
  RotateCcw,
} from "lucide-react";
import { call } from "@saucer-dev/types";
import { useState } from "react";

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

function StepStatusIcon({ status }: { status: StepStatus }) {
  switch (status) {
    case StepStatus.Success:
      return <CheckCircle className="h-4 w-4 text-green-500" />;
    case StepStatus.Failed:
      return <XCircle className="h-4 w-4 text-red-500" />;
    case StepStatus.Running:
      return <Loader2 className="h-4 w-4 text-blue-500 animate-spin" />;
    case StepStatus.TimedOut:
      return <Clock className="h-4 w-4 text-amber-500" />;
    case StepStatus.Skipped:
      return <SkipForward className="h-4 w-4 text-gray-400" />;
    case StepStatus.Aborted:
      return <AlertTriangle className="h-4 w-4 text-red-400" />;
    case StepStatus.Pending:
    default:
      return <div className="h-4 w-4 rounded-full border-2 border-gray-300" />;
  }
}

function getStepStatusLabel(status: StepStatus): string {
  switch (status) {
    case StepStatus.Pending:
      return "Pending";
    case StepStatus.Running:
      return "Running";
    case StepStatus.Success:
      return "Success";
    case StepStatus.Failed:
      return "Failed";
    case StepStatus.TimedOut:
      return "Timed Out";
    case StepStatus.Skipped:
      return "Skipped";
    case StepStatus.Aborted:
      return "Aborted";
    default:
      return "Unknown";
  }
}

const TEST_MODES = [
  { value: 0, label: "Normal", description: "All steps succeed" },
  { value: 1, label: "With Failure", description: "One step fails" },
  { value: 2, label: "With Timeout", description: "One step times out" },
] as const;

export function StartupStatus() {
  const { progress, appState, isComplete, error } = useStartupEvents();
  const [testMode, setTestMode] = useState(0);

  const progressPercent = progress
    ? ((progress.current_step_index + 1) / Math.max(progress.steps.length, 1)) *
      100
    : 0;

  const handleStart = async () => {
    await call<boolean>("start_startup", []);
  };

  const handleAbort = async () => {
    await call<void>("abort_startup", []);
  };

  const handleRetry = async () => {
    await call<boolean>("retry_startup", []);
  };

  const handleTestModeChange = async (mode: number) => {
    setTestMode(mode);
    await call<void>("set_test_mode", [mode]);
  };

  const isRunning = appState === AppState.Booting;
  const canStart = appState === AppState.Off;
  const canRetry = progress?.can_retry && !isRunning;

  return (
    <Card className="w-full max-w-md">
      <CardHeader>
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

      <CardContent className="space-y-4">
        {/* Test mode selector - only show when not running */}
        {canStart && (
          <div className="space-y-2">
            <label className="text-sm font-medium">Test Mode</label>
            <div className="flex gap-1">
              {TEST_MODES.map((mode) => (
                <Button
                  key={mode.value}
                  variant={testMode === mode.value ? "default" : "outline"}
                  size="sm"
                  onClick={() => handleTestModeChange(mode.value)}
                  title={mode.description}
                >
                  {mode.label}
                </Button>
              ))}
            </div>
          </div>
        )}

        {/* Control buttons */}
        <div className="flex gap-2">
          {canStart && (
            <Button onClick={handleStart} className="flex-1">
              <Play className="h-4 w-4 mr-2" />
              Start
            </Button>
          )}
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
            <Button onClick={handleRetry} variant="outline" className="flex-1">
              <RotateCcw className="h-4 w-4 mr-2" />
              Retry
            </Button>
          )}
        </div>

        {/* Progress bar */}
        {isRunning && (
          <div className="space-y-2">
            <div className="flex justify-between text-sm text-muted-foreground">
              <span>Progress</span>
              <span>{Math.round(progressPercent)}%</span>
            </div>
            <Progress value={progressPercent} />
          </div>
        )}

        {/* Step list */}
        {progress && progress.steps.length > 0 && (
          <div className="space-y-2">
            <h4 className="text-sm font-medium">Steps</h4>
            <ul className="space-y-1">
              {progress.steps.map((step, index) => (
                <li
                  key={step.id}
                  className={`flex items-center gap-2 text-sm p-2 rounded ${
                    index === progress.current_step_index
                      ? "bg-accent"
                      : "bg-transparent"
                  }`}
                >
                  <StepStatusIcon status={step.status} />
                  <span className="flex-1">{step.display_name}</span>
                  <span className="text-xs text-muted-foreground">
                    {getStepStatusLabel(step.status)}
                  </span>
                </li>
              ))}
            </ul>
          </div>
        )}

        {/* Error display */}
        {error && (
          <div className="p-3 bg-destructive/10 border border-destructive/20 rounded-md">
            <p className="text-sm text-destructive">{error}</p>
          </div>
        )}

        {/* Debug info */}
        <details className="text-xs text-muted-foreground">
          <summary className="cursor-pointer">Debug Info</summary>
          <pre className="mt-2 p-2 bg-muted rounded overflow-auto max-h-40">
            {JSON.stringify(
              { appState, testMode, progress, isComplete, error },
              null,
              2,
            )}
          </pre>
        </details>
      </CardContent>
    </Card>
  );
}
