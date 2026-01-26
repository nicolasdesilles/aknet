//
// Created by Nicolas Désilles on 26/01/2026.
//

#ifndef AKNET_COREAUDIO_DEVICE_MANAGER_H
#define AKNET_COREAUDIO_DEVICE_MANAGER_H

#pragma once

#ifdef __APPLE__

#include "audio_device_manager.h"
#include <logger.h>
#include <memory>

namespace aknet::jack {

    /**
     * macOS CoreAudio implementation of IAudioDeviceManager.
     *
     * Uses CoreAudio APIs to enumerate hardware devices and query their capabilities.
     * Device IDs are the human-readable device names from CoreAudio.
     *
     * #### Platform Support
     *
     * - macOS: Full support
     * - Linux/Windows: **NOT AVAILABLE** (compile-time excluded)
     *
     * #### Device Naming
     *
     * CoreAudio device UIDs are long UUID-like strings. Instead, we use the device name
     * as the ID for better UX in settings. JACK's CoreAudio driver accepts device names
     * via the -C and -P flags.
     *
     */
    class CoreAudioDeviceManager : public IAudioDeviceManager {
    public:
        /**
         * Construct a CoreAudioDeviceManager.
         *
         * @param logger Logger for diagnostic output.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         */
        explicit CoreAudioDeviceManager(std::shared_ptr<log::Logger> logger);

        ~CoreAudioDeviceManager() override = default;

        // IAudioDeviceManager interface
        std::vector<AudioDevice> enumerate_devices() override;
        std::optional<AudioDevice> get_device_by_id(const std::string& id) override;
        AudioDevice get_default_device() override;

    private:
        std::shared_ptr<log::Logger> logger_;

        /**
         * Cache of enumerated devices.
         * Updated on each enumerate_devices() call.
         */
        std::vector<AudioDevice> cached_devices_;

        /**
         * Query CoreAudio for all available audio devices.
         * Populates cached_devices_.
         */
        void refresh_device_cache();

        /**
         * Get the name of a CoreAudio device.
         *
         * @param device_id CoreAudio AudioDeviceID.
         * @return Device name, or empty string on error.
         */
        std::string get_device_name(unsigned int device_id);

        /**
         * Get the number of input channels for a device.
         *
         * @param device_id CoreAudio AudioDeviceID.
         * @return Number of input channels, or 0 on error.
         */
        int get_input_channel_count(unsigned int device_id);

        /**
         * Get the number of output channels for a device.
         *
         * @param device_id CoreAudio AudioDeviceID.
         * @return Number of output channels, or 0 on error.
         */
        int get_output_channel_count(unsigned int device_id);

        /**
         * Check if a device is the system default output device.
         *
         * @param device_id CoreAudio AudioDeviceID.
         * @return True if this is the default output device.
         */
        bool is_default_device(unsigned int device_id);
    };

} // namespace aknet::jack

#endif // __APPLE__

#endif //AKNET_COREAUDIO_DEVICE_MANAGER_H