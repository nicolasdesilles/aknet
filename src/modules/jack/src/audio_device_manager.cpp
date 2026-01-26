//
// Created by Nicolas Désilles on 26/01/2026.
//

#include "audio_device_manager.h"

#ifdef __APPLE__
#include "coreaudio_device_manager.h"
#endif

#include <stdexcept>

namespace aknet::jack {

    std::shared_ptr<IAudioDeviceManager> create_audio_device_manager(
        std::shared_ptr<log::Logger> logger)
    {
        if (!logger) {
            throw std::invalid_argument("Logger cannot be null");
        }

#ifdef __APPLE__
        return std::make_shared<CoreAudioDeviceManager>(logger);
#else
        // Future: Add ALSA for Linux, WASAPI/ASIO for Windows
        throw std::runtime_error("Audio device enumeration not supported on this platform");
#endif
    }

} // namespace aknet::jack