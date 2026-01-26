//
// Created by Nicolas Désilles on 26/01/2026.
//

#ifdef __APPLE__

#include "coreaudio_device_manager.h"
#include <stdexcept>
#include <CoreAudio/CoreAudio.h>

namespace aknet::jack {

CoreAudioDeviceManager::CoreAudioDeviceManager(std::shared_ptr<log::Logger> logger)
    : logger_(std::move(logger))
{
    if (!logger_) {
        throw std::invalid_argument("Logger cannot be null");
    }
}

std::vector<AudioDevice> CoreAudioDeviceManager::enumerate_devices() {
    logger_->debug("Enumerating CoreAudio devices");

    refresh_device_cache();

    logger_->info("Found {} audio devices", cached_devices_.size());
    return cached_devices_;
}

std::optional<AudioDevice> CoreAudioDeviceManager::get_device_by_id(const std::string& id) {
    logger_->debug("Looking up device by ID: {}", id);

    // Special case: "system_default" means current system default
    if (id == "system_default") {
        auto default_device = get_default_device();
        // Return it as the sentinel but with current system info
        return AudioDevice{
            .id = "system_default",  // Keep the sentinel ID
            .name = "System Default (" + default_device.name + ")",
            .input_channels = default_device.input_channels,
            .output_channels = default_device.output_channels,
            .is_default = true
        };
    }

    // Refresh cache to ensure we have latest devices
    refresh_device_cache();

    for (const auto& device : cached_devices_) {
        if (device.id == id) {
            logger_->debug("Found device: {}", device.name);
            return device;
        }
    }

    logger_->warn("Device not found: {}", id);
    return std::nullopt;
}

AudioDevice CoreAudioDeviceManager::get_default_device() {
    logger_->debug("Getting system default output device");

    // Query CoreAudio for the actual default output device
    AudioObjectPropertyAddress property_address = {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    AudioDeviceID default_device_id = kAudioDeviceUnknown;
    UInt32 data_size = sizeof(default_device_id);

    OSStatus status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &property_address,
        0,
        nullptr,
        &data_size,
        &default_device_id
    );

    if (status != noErr || default_device_id == kAudioDeviceUnknown) {
        logger_->error("Failed to get system default device: {}", status);

        // Fallback: return a minimal valid device
        return {
            .id = "unknown",
            .name = "Unknown Default Device",
            .input_channels = 2,
            .output_channels = 2,
            .is_default = true
        };
    }

    // Get the actual device properties
    std::string name = get_device_name(default_device_id);
    int input_channels = get_input_channel_count(default_device_id);
    int output_channels = get_output_channel_count(default_device_id);

    logger_->info("System default device: {} ({} in, {} out)", name, input_channels, output_channels);

    return {
        .id = name,  // Use the actual device name as ID
        .name = name,
        .input_channels = input_channels,
        .output_channels = output_channels,
        .is_default = true
    };
}

void CoreAudioDeviceManager::refresh_device_cache() {
    cached_devices_.clear();

    // First, add the "system_default" sentinel with current system default info
    auto current_default = get_default_device();
    cached_devices_.push_back({
        .id = "system_default",  // Special sentinel ID
        .name = "System Default (" + current_default.name + ")",  // Show what it currently is
        .input_channels = current_default.input_channels,
        .output_channels = current_default.output_channels,
        .is_default = true
    });

    // Query CoreAudio for device list
    AudioObjectPropertyAddress property_address = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject,
        &property_address,
        0,
        nullptr,
        &data_size
    );

    if (status != noErr) {
        logger_->error("Failed to get CoreAudio device list size: {}", status);
        return;
    }

    UInt32 device_count = data_size / sizeof(AudioDeviceID);
    if (device_count == 0) {
        logger_->warn("No CoreAudio devices found");
        return;
    }

    std::vector<AudioDeviceID> device_ids(device_count);
    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &property_address,
        0,
        nullptr,
        &data_size,
        device_ids.data()
    );

    if (status != noErr) {
        logger_->error("Failed to get CoreAudio device list: {}", status);
        return;
    }

    // Get the actual system default device ID for marking
    AudioDeviceID system_default_id = kAudioDeviceUnknown;
    AudioObjectPropertyAddress default_property = {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };
    UInt32 default_size = sizeof(system_default_id);
    AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &default_property,
        0,
        nullptr,
        &default_size,
        &system_default_id
    );

    // Enumerate each device
    for (AudioDeviceID device_id : device_ids) {
        std::string name = get_device_name(device_id);
        if (name.empty()) {
            logger_->warn("Skipping device with empty name: {}", device_id);
            continue;
        }

        int input_channels = get_input_channel_count(device_id);
        int output_channels = get_output_channel_count(device_id);

        // Skip devices with no channels (transport-only devices)
        if (input_channels == 0 && output_channels == 0) {
            logger_->debug("Skipping device with no audio channels: {}", name);
            continue;
        }

        AudioDevice device{
            .id = name,  // Use name as ID for JACK compatibility
            .name = name,
            .input_channels = input_channels,
            .output_channels = output_channels,
            .is_default = (device_id == system_default_id)  // Mark actual system default
        };

        logger_->debug("  - {} ({} in, {} out){}",
                      name,
                      input_channels,
                      output_channels,
                      device.is_default ? " [SYSTEM DEFAULT]" : "");
        cached_devices_.push_back(device);
    }
}

std::string CoreAudioDeviceManager::get_device_name(unsigned int device_id) {
    AudioObjectPropertyAddress property_address = {
        .mSelector = kAudioDevicePropertyDeviceNameCFString,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    CFStringRef cf_name = nullptr;
    UInt32 data_size = sizeof(cf_name);

    OSStatus status = AudioObjectGetPropertyData(
        device_id,
        &property_address,
        0,
        nullptr,
        &data_size,
        &cf_name
    );

    if (status != noErr || !cf_name) {
        return "";
    }

    // Convert CFString to std::string
    char buffer[256];
    Boolean success = CFStringGetCString(
        cf_name,
        buffer,
        sizeof(buffer),
        kCFStringEncodingUTF8
    );

    CFRelease(cf_name);

    return success ? std::string(buffer) : "";
}

int CoreAudioDeviceManager::get_input_channel_count(unsigned int device_id) {
    AudioObjectPropertyAddress property_address = {
        .mSelector = kAudioDevicePropertyStreamConfiguration,
        .mScope = kAudioDevicePropertyScopeInput,
        .mElement = kAudioObjectPropertyElementMain
    };

    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(
        device_id,
        &property_address,
        0,
        nullptr,
        &data_size
    );

    if (status != noErr) {
        return 0;
    }

    std::vector<uint8_t> buffer(data_size);
    status = AudioObjectGetPropertyData(
        device_id,
        &property_address,
        0,
        nullptr,
        &data_size,
        buffer.data()
    );

    if (status != noErr) {
        return 0;
    }

    auto* buffer_list = reinterpret_cast<AudioBufferList*>(buffer.data());
    int total_channels = 0;

    for (UInt32 i = 0; i < buffer_list->mNumberBuffers; ++i) {
        total_channels += buffer_list->mBuffers[i].mNumberChannels;
    }

    return total_channels;
}

int CoreAudioDeviceManager::get_output_channel_count(unsigned int device_id) {
    AudioObjectPropertyAddress property_address = {
        .mSelector = kAudioDevicePropertyStreamConfiguration,
        .mScope = kAudioDevicePropertyScopeOutput,
        .mElement = kAudioObjectPropertyElementMain
    };

    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(
        device_id,
        &property_address,
        0,
        nullptr,
        &data_size
    );

    if (status != noErr) {
        return 0;
    }

    std::vector<uint8_t> buffer(data_size);
    status = AudioObjectGetPropertyData(
        device_id,
        &property_address,
        0,
        nullptr,
        &data_size,
        buffer.data()
    );

    if (status != noErr) {
        return 0;
    }

    auto* buffer_list = reinterpret_cast<AudioBufferList*>(buffer.data());
    int total_channels = 0;

    for (UInt32 i = 0; i < buffer_list->mNumberBuffers; ++i) {
        total_channels += buffer_list->mBuffers[i].mNumberChannels;
    }

    return total_channels;
}

bool CoreAudioDeviceManager::is_default_device(unsigned int device_id) {
    AudioObjectPropertyAddress property_address = {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    AudioDeviceID default_device_id = kAudioDeviceUnknown;
    UInt32 data_size = sizeof(default_device_id);

    OSStatus status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &property_address,
        0,
        nullptr,
        &data_size,
        &default_device_id
    );

    if (status != noErr) {
        return false;
    }

    return device_id == default_device_id;
}

} // namespace aknet::jack

#endif // __APPLE__