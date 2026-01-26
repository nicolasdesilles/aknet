export interface AudioDevice {
  id: string;
  name: string;
  input_channels: number;
  output_channels: number;
  is_default: boolean;
}

export interface AudioDevicesResponse {
  devices?: AudioDevice[];
  error?: string;
}
