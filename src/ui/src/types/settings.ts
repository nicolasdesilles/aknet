// Valid audio configuration values
export const VALID_SAMPLE_RATES = [
  44100, 48000, 88200, 96000, 176400, 192000,
] as const;

export const VALID_BUFFER_SIZES = [32, 64, 128, 256, 512, 1024, 2048] as const;

export interface General {
  log_level: string;
  test_restart_impact: number;
}

export interface Audio {
  sampling_rate: number;
  buffer_size: number;
  num_channels: number;
  input_device_id: string;
  output_device_id: string;
}

export interface Jack {
  client_name: string;
  server_executable_path: string;
  auto_manage_server: boolean;
}

export interface AppSettings {
  schema_version: number;
  general: General;
  audio: Audio;
  jack: Jack;
}

export interface Result {
  ok: boolean;
  error: string;
}

export interface SaveImpact {
  app_restart_required: boolean;
  modules_restart_required: string[];
  restart_sensitive_keys_changed: string[];
}

export interface SaveResult {
  result: Result;
  save_impact: SaveImpact;
}
