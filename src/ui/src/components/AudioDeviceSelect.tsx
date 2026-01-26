import { useState, useEffect } from "react";
import { call } from "@saucer-dev/types";
import type { AudioDevice } from "@/types/devices";

import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";

interface AudioDeviceSelectProps {
  value: string;
  onChange: (value: string) => void;
  filterType?: "input" | "output" | "all";
  disabled?: boolean;
}

export function AudioDeviceSelect({
  value,
  onChange,
  filterType = "all",
  disabled = false,
}: AudioDeviceSelectProps) {
  const [devices, setDevices] = useState<AudioDevice[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    loadDevices();
  }, []);

  const loadDevices = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const devicesJson = await call<string>("get_audio_devices", []);
      const parsed = JSON.parse(devicesJson);

      if (parsed.error) {
        setError(parsed.error);
        setDevices([]);
      } else if (Array.isArray(parsed)) {
        // Response is directly an array
        setDevices(parsed);
      } else if (parsed.devices) {
        // Response has devices property
        setDevices(parsed.devices);
      } else {
        setDevices([]);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to load devices");
      setDevices([]);
    } finally {
      setIsLoading(false);
    }
  };

  const filteredDevices = devices.filter((device) => {
    if (filterType === "input") {
      return device.id === "system_default" || device.input_channels > 0;
    }
    if (filterType === "output") {
      return device.id === "system_default" || device.output_channels > 0;
    }
    return true; // "all"
  });

  const getDeviceLabel = (device: AudioDevice): string => {
    if (device.id === "system_default") {
      return device.name;
    }

    const channels = [];
    if (device.input_channels > 0) {
      channels.push(`${device.input_channels} in`);
    }
    if (device.output_channels > 0) {
      channels.push(`${device.output_channels} out`);
    }

    const channelInfo = channels.length > 0 ? ` (${channels.join(", ")})` : "";
    return `${device.name}${channelInfo}`;
  };

  if (isLoading) {
    return (
      <Select disabled>
        <SelectTrigger>
          <SelectValue placeholder="Loading devices..." />
        </SelectTrigger>
      </Select>
    );
  }

  if (error) {
    return (
      <Select disabled>
        <SelectTrigger>
          <SelectValue placeholder={`Error: ${error}`} />
        </SelectTrigger>
      </Select>
    );
  }

  if (filteredDevices.length === 0) {
    return (
      <Select disabled>
        <SelectTrigger>
          <SelectValue placeholder="No devices available" />
        </SelectTrigger>
      </Select>
    );
  }

  return (
    <Select value={value} onValueChange={onChange} disabled={disabled}>
      <SelectTrigger>
        <SelectValue placeholder="Select audio device" />
      </SelectTrigger>
      <SelectContent>
        {filteredDevices.map((device) => (
          <SelectItem key={device.id} value={device.id}>
            {getDeviceLabel(device)}
          </SelectItem>
        ))}
      </SelectContent>
    </Select>
  );
}
