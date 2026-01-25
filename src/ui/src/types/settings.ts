export interface General {
  log_level: string;
  test_restart_impact: number;
}

export interface Audio {
  sampling_rate: number;
  buffer_size: number;
  num_channels: number;
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
