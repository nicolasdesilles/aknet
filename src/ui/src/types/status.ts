export type AppStatus =
  | "off"
  | "booting"
  | "active"
  | "shutting_down"
  | "error";

export interface AppStatusSnapshot {
  status: AppStatus;
}

export interface JackServerStatus {
  running: boolean;
  pid: number | null;
}

export interface JackClientStatus {
  connected: boolean;
  channels: number;
}

export interface AudioSettingsStatus {
  sampleRate: number;
  bufferSize: number;
}

export interface JackRuntimeStatus {
  cpuLoad: number | null;
}

export interface StatusSnapshot {
  app: AppStatusSnapshot;
  jackServer: JackServerStatus;
  jackClient: JackClientStatus;
  audio: AudioSettingsStatus;
  jackRuntime: JackRuntimeStatus;
}

export interface StatusSnapshotUpdate {
  app?: Partial<AppStatusSnapshot>;
  jackServer?: Partial<JackServerStatus>;
  jackClient?: Partial<JackClientStatus>;
  audio?: Partial<AudioSettingsStatus>;
  jackRuntime?: Partial<JackRuntimeStatus>;
}
