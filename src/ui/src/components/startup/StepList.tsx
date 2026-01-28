import { useEffect, useRef } from "react";
import { type StepProgress, StepStatus } from "@/types/startup";
import {
  CheckCircle,
  XCircle,
  Loader2,
  Clock,
  SkipForward,
  AlertTriangle,
} from "lucide-react";

interface StepListProps {
  steps: StepProgress[];
  currentStepIndex: number;
}

function StepStatusIcon({ status }: { status: StepStatus }) {
  switch (status) {
    case StepStatus.Success:
      return <CheckCircle className="h-4 w-4 text-green-500 flex-shrink-0" />;
    case StepStatus.Failed:
      return <XCircle className="h-4 w-4 text-red-500 flex-shrink-0" />;
    case StepStatus.Running:
      return (
        <Loader2 className="h-4 w-4 text-blue-500 animate-spin flex-shrink-0" />
      );
    case StepStatus.TimedOut:
      return <Clock className="h-4 w-4 text-amber-500 flex-shrink-0" />;
    case StepStatus.Skipped:
      return <SkipForward className="h-4 w-4 text-gray-400 flex-shrink-0" />;
    case StepStatus.Aborted:
      return <AlertTriangle className="h-4 w-4 text-red-400 flex-shrink-0" />;
    case StepStatus.Pending:
    default:
      return (
        <div className="h-4 w-4 rounded-full border-2 border-gray-400 flex-shrink-0" />
      );
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

export function StepList({ steps, currentStepIndex }: StepListProps) {
  const currentStepRef = useRef<HTMLLIElement>(null);

  // Auto-scroll to current step
  useEffect(() => {
    if (currentStepRef.current) {
      currentStepRef.current.scrollIntoView({
        behavior: "smooth",
        block: "center",
      });
    }
  }, [currentStepIndex]);

  if (steps.length === 0) {
    return null;
  }

  return (
    <div className="flex flex-col min-h-0 flex-1">
      <h4 className="text-sm font-medium mb-2 px-1">Steps</h4>
      <ul className="step-list flex-1 overflow-y-auto space-y-1 pr-2">
        {steps.map((step, index) => (
          <li
            key={step.id}
            ref={index === currentStepIndex ? currentStepRef : null}
            className={`
              flex items-center gap-2 text-sm p-2 rounded-md
              transition-all duration-200
              ${
                index === currentStepIndex
                  ? "bg-accent/50 ring-1 ring-accent/30"
                  : "bg-transparent hover:bg-accent/10"
              }
            `}
          >
            <StepStatusIcon status={step.status} />
            <span className="flex-1 truncate">{step.display_name}</span>
            <span className="text-xs text-muted-foreground whitespace-nowrap">
              {getStepStatusLabel(step.status)}
            </span>
          </li>
        ))}
      </ul>
    </div>
  );
}
