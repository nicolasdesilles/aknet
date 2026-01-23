// Matches C++ enum class AppState { Off, Booting, Active, ShuttingDown }
export const AppState = {
  Off: 0,
  Booting: 1,
  Active: 2,
  ShuttingDown: 3,
} as const;
export type AppState = (typeof AppState)[keyof typeof AppState];

// Matches C++ enum class StepStatus { Pending, Running, Success, Failed, TimedOut, Skipped, Aborted }
export const StepStatus = {
  Pending: 0,
  Running: 1,
  Success: 2,
  Failed: 3,
  TimedOut: 4,
  Skipped: 5,
  Aborted: 6,
} as const;
export type StepStatus = (typeof StepStatus)[keyof typeof StepStatus];

export interface StepProgress {
  id: string;
  display_name: string;
  status: StepStatus;
  message: string;
}

export interface StartupProgress {
  state: AppState;
  current_step_index: number;
  steps: StepProgress[];
  last_error?: string;
  can_retry: boolean;
}

export interface StateChangedEvent {
  old_state: AppState;
  new_state: AppState;
}

export interface StepStartedEvent {
  index: number;
  id: string;
}

export interface StepCompletedEvent {
  index: number;
  id: string;
  status: StepStatus;
}

export interface SequenceCompletedEvent {
  success: boolean;
  error: string;
}

export interface ErrorEvent {
  message: string;
}
