//
// Created by Nicolas Désilles on 26/01/2026.
//

#ifndef AKNET_AUDIO_DEVICE_MANAGER_H
#define AKNET_AUDIO_DEVICE_MANAGER_H

#pragma once

#include "jack_interfaces.h"
#include <logger.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aknet::jack {

    /**
     * Represents an audio device available on the system.
     *
     * Contains hardware-specific information needed for JACK device selection.
     */
    struct AudioDevice {
        std::string id;              ///< Unique device identifier
        std::string name;            ///< Human-readable device name
        int input_channels = 0;      ///< Number of available input channels
        int output_channels = 0;     ///< Number of available output channels
        bool is_default = false;     ///< True if this is the system default device

        /**
         * Check if device can be used for input.
         * @return True if device has at least one input channel.
         */
        bool can_capture() const {
            return input_channels > 0;
        }

        /**
         * Check if device can be used for output.
         * @return True if device has at least one output channel.
         */
        bool can_playback() const {
            return output_channels > 0;
        }
    };

    /**
     * Interface for enumerating and querying audio devices on the system.
     *
     * This abstraction allows platform-specific implementations (CoreAudio, ALSA, WASAPI)
     * while keeping the JACK module platform-agnostic.
     *
     * #### Special Device ID: "system_default"
     *
     * The ID "system_default" is a sentinel value meaning "use the operating system's
     * current default audio device". This device should always be present in the enumeration.
     * When "system_default" is selected, JACK will use its driver's default device selection.
     *
     * #### Thread Safety
     *
     * NOT thread-safe. All methods should be called from the same thread.
     */
    class IAudioDeviceManager {
    public:
        virtual ~IAudioDeviceManager() = default;

        /**
         * Enumerate all available audio devices on the system.
         *
         * The returned list always includes a special "system_default" device as the first entry.
         * Subsequent entries are actual hardware devices discovered on the system.
         *
         * @return Vector of audio devices (empty only if enumeration fails catastrophically).
         */
        virtual std::vector<AudioDevice> enumerate_devices() = 0;

        /**
         * Get a specific device by its ID.
         *
         * @param id Device identifier to look up.
         * @return AudioDevice if found, std::nullopt otherwise.
         */
        virtual std::optional<AudioDevice> get_device_by_id(const std::string& id) = 0;

        /**
         * Get the special "system_default" device.
         *
         * This is a convenience method equivalent to get_device_by_id("system_default").
         *
         * @return The system default device entry.
         */
        virtual AudioDevice get_default_device() = 0;
    };

    /**
     * Factory function to create the platform-specific audio device manager.
     *
     * Currently creates CoreAudioDeviceManager on macOS.
     * Future: ALSA on Linux, WASAPI/ASIO on Windows.
     *
     * @param logger Logger for diagnostic output.
     * @return Platform-appropriate IAudioDeviceManager implementation.
     *
     * #### Exceptions
     *
     * - Throws `std::invalid_argument` if logger is null.
     */
    std::shared_ptr<IAudioDeviceManager> create_audio_device_manager(
        std::shared_ptr<log::Logger> logger
    );

} // namespace aknet::jack

#endif //AKNET_AUDIO_DEVICE_MANAGER_H