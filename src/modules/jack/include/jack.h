//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_JACK_H
#define AKNET_JACK_H

#include "jack_interfaces.h"
#include "audio_device_manager.h"
#include <logger.h>
#include <memory>

#pragma once

namespace aknet::jack {

    /**
     * Factory function to create a platform-appropriate ProcessRunner.
     *
     * Returns:
     * - PosixProcessRunner on Unix-like systems (macOS, Linux, BSD)
     * - WindowsProcessRunner on Windows (when implemented later)
     *
     * #### Future Windows Support
     *
     * When Windows support is added, this function will automatically
     * return the correct implementation based on compile-time platform detection.
     *
     * @param logger Logger instance.
     * @return Shared pointer to IProcessRunner.
     */
    std::shared_ptr<IProcessRunner> create_process_runner(
        std::shared_ptr<log::Logger> logger
    );

    /**
     * Factory function to create a real LibJackClientAPI.
     *
     * Wraps libjack C API in a C++ interface.
     *
     * @param logger Logger instance.
     * @return Shared pointer to LibJackClientAPI.
     */
    std::shared_ptr<IJackClientAPI> create_libjack_client_api(
        std::shared_ptr<log::Logger> logger
    );

    /**
     * Factory function to create a platform-appropriate AudioDeviceManager.
     *
     * Returns:
     * - CoreAudioDeviceManager on macOS
     * - AlsaDeviceManager on Linux (future)
     * - WasapiDeviceManager on Windows (future)
     *
     * @param logger Logger instance.
     * @return Shared pointer to IAudioDeviceManager.
     */
    std::shared_ptr<IAudioDeviceManager> create_device_manager(
        std::shared_ptr<log::Logger> logger
    );

}

#endif //AKNET_JACK_H