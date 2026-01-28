export type PanelId = "audio-levels" | "networking" | "settings";

export type AppScreen = "startup" | "transitioning" | "main";

export interface PanelConfig {
  id: PanelId;
  label: string;
  icon: string;
}

export const PANELS: PanelConfig[] = [
  { id: "audio-levels", label: "Audio Levels", icon: "AudioLines" },
  { id: "networking", label: "Networking", icon: "Network" },
  { id: "settings", label: "Settings", icon: "Settings" },
];
