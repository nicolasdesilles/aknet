//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_JACK_MODULE_H
#define AKNET_JACK_MODULE_H

#pragma once

#include "jack_interfaces.h"
#include "jack_server_manager.h"
#include "jack_client.h"
#include "jack_audio_processor.h"
#include "audio_device_manager.h"

#include <logger.h>
#include <settings.h>
#include <memory>
#include <optional>
#include <vector>

namespace aknet::jack {
    /**
     * State of the JACK module.
     */
    enum class JackModuleState {
        Uninitialized,  ///< Not yet initialized
        Initialized,    ///< Initialized with settings, but not active
        Active          ///< JACK client active and processing audio
    };

    /**
    * High-level facade for JACK audio integration.
    *
    * Manages the complete JACK lifecycle:
    * 1. Initialize with application settings
    * 2. Start JACK server (if needed) and register client
    * 3. Process audio and provide level meters
    * 4. Stop and cleanup
    *
    * #### Lifecycle
    *
    * ```cpp
    * JackModule module(logger);
    * module.init(settings); // Uninitialized → Initialized
    * module.start(); // Initialized → Active
    * // ... audio processing ...
    * module.stop(); // Active → Initialized
    * ```
    *
    * #### Thread Safety
    *
    * - NOT thread-safe for lifecycle methods (init/start/stop)
    * - Audio level queries ARE thread-safe (can call from UI thread)
    *
    * @see JackServerManager
    * @see JackClient
    * @see JackAudioProcessor
    */

    class JackModule {
    public:
        /**
         * Construct a JACK module.
         *
         * @param logger Logger for diagnostic output.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         */
        explicit JackModule(std::shared_ptr<log::Logger> logger);

        ~JackModule() = default;

        // Non-copyable, non-movable
        JackModule(const JackModule&) = delete;
        JackModule& operator=(const JackModule&) = delete;

        /**
         * Initialize the module with application settings.
         *
         * Creates internal components (server manager, client, processor)
         * but does not start the JACK server or client yet.
         *
         * @param settings Application settings containing JACK and audio config.
         * @param process_runner Process runner for spawning jackd (optional, uses default if null).
         * @param client_api JACK client API (optional, uses default if null).
         *
         * @return Result indicating success or error.
         *
         * #### Errors
         *
         * - Module already initialized
         * - Invalid settings (e.g., num_channels < 1)
         */
        Result init(
            const settings::AppSettings& settings,
            std::shared_ptr<IProcessRunner> process_runner = nullptr,
            std::shared_ptr<IJackClientAPI> client_api = nullptr,
            std::shared_ptr<IAudioDeviceManager> device_manager = nullptr
        );

        /**
         * Start JACK server and client.
         *
         * This will:
         * 1. Ensure JACK server is running with correct settings
         * 2. Open JACK client
         * 3. Register input ports
         * 4. Set audio processor
         * 5. Activate client (start audio processing)
         *
         * @return Result indicating success or error.
         *
         * #### Errors
         *
         * - Module not initialized
         * - JACK server failed to start
         * - Client registration failed
         */
        Result start();

        /**
         * Stop JACK client.
         *
         * Closes the client and deactivates audio processing.
         * Does not stop the JACK server (other apps may be using it).
         *
         * @return Result indicating success or error.
         */
        Result stop();

        /**
         * Get current audio levels (RMS) for all channels.
         *
         * Thread-safe: can be called from UI thread.
         *
         * @return Vector of RMS levels in dB, one per channel. Returns empty vector if not active.
         */
        std::vector<float> get_audio_levels() const;

        /**
         * Get current peak levels for all channels.
         *
         * Thread-safe: can be called from UI thread.
         *
         * @return Vector of peak levels in dB, one per channel.
         *         Returns empty vector if not active.
         */
        std::vector<float> get_peak_levels() const;

        /**
         * Reset peak hold meters to -inf dB.
         *
         * Thread-safe: can be called from UI thread.
         */
        void reset_peaks();

        /**
         * Check if module is ready for audio processing.
         *
         * @return True if state is Active.
         */
        bool is_ready() const;

        /**
         * Get current module state.
         *
         * @return Current state (Uninitialized, Initialized, or Active).
         */
        JackModuleState get_state() const;

        /**
         * Check if the JACK client is active.
         *
         * @return True if client is active.
         */
        bool is_client_active() const;

        /**
         * Get the current number of input ports registered on the client.
         *
         * @return Number of input ports, or 0 if not available.
         */
        int get_client_input_port_count() const;


    private:
        std::shared_ptr<log::Logger> logger_;

        JackModuleState state_ = JackModuleState::Uninitialized;

        // Components (created during init)
        std::unique_ptr<JackServerManager> server_manager_;
        std::unique_ptr<JackClient> client_;
        std::shared_ptr<JackAudioProcessor> audio_processor_;
        std::shared_ptr<IAudioDeviceManager> device_manager_;

        // Settings cache
        settings::AppSettings settings_;

        /**
         * Validate that audio devices in settings exist and are usable.
         *
         * Checks that:
         * - Device IDs resolve to actual devices (or are "system_default")
         * - Input device has input capability
         * - Output device has output capability
         *
         * @param[in,out] settings Settings to validate (may be modified on fallback).
         * @param fallback_to_default If true, fall back to system_default on missing devices.
         *
         * @return Result indicating success or validation error.
         */
        Result validate_and_fallback_devices(
            settings::AppSettings& settings,
            bool fallback_to_default = true
        );
    };

}

#endif //AKNET_JACK_MODULE_H