//
// Created by Nicolas Désilles on 31/01/2026.
//

#ifndef AKNET_AUDIO_MODULE_H
#define AKNET_AUDIO_MODULE_H

#pragma once

#include "audio_block_view.h"
#include "audio_buffer.h"
#include <logger.h>
#include <settings.h>
#include <memory>

namespace aknet::audio {
    /**
     * Result type for audio operations.
     */
    struct Result {
        bool ok = true;
        std::string error;
    };

    /**
     * State of the audio module.
     */
    enum class AudioModuleState {
        Uninitialized,  ///< Not yet initialized
        Initialized,    ///< Initialized with settings, but not active
        Active          ///< Ready for processing audio
    };

    /**
    * High-level facade for audio processing and metering.
    *
    * Manages:
    * 1. Audio buffer storage (non-RT access to processed audio)
    * 2. Pre-DSP and post-DSP metering
    * 3. Per-channel FIFOs for drift mitigation
    * 4. Gain/mute processing
    *
    * #### Lifecycle
    *
    * ```cpp
    * AudioModule module(logger);
    * module.init(settings); // Uninitialized → Initialized
    * module.start(); // Initialized → Active
    * // ... RT callback calls process_block() ...
    * module.stop(); // Active → Initialized
    * ```
    * #### RT Entry Point
    *
    * The `process_block()` method is the RT-safe entry point called from
    * the JACK callback thread. It performs:
    * - Pre-DSP metering
    * - Gain/mute processing
    * - Copying to owned buffers
    * - Post-DSP metering
    * - Writing to FIFOs
    *
    * #### Thread Safety
    *
    * - Lifecycle methods (init/start/stop) are NOT thread-safe
    * - `process_block()` is RT-safe and lock-free
    * - Meter queries ARE thread-safe (atomics)
    *
    * @see AudioBlockView
    * @see AudioBuffer
    */
    class AudioModule {
    public:
        /**
         * Construct an audio module.
         *
         * @param logger Logger for diagnostic output.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         */
        explicit AudioModule(std::shared_ptr<log::Logger> logger);

        ~AudioModule() = default;

        // Non-copyable, non-movable
        AudioModule(const AudioModule&) = delete;
        AudioModule& operator=(const AudioModule&) = delete;

        /**
         * Initialize the module with application settings.
         *
         * Preallocates buffers based on settings (channels, buffer size).
         * Does not start processing yet.
         *
         * @param settings Application settings containing audio config.
         *
         * @return Result indicating success or error.
         *
         * #### Errors
         *
         * - Module already initialized
         * - Invalid settings (e.g., num_channels < 1)
         */
        Result init(const settings::AppSettings& settings);

        /**
         * Start audio processing.
         *
         * Transitions to Active state, ready to process audio blocks.
         *
         * @return Result indicating success or error.
         *
         * #### Errors
         *
         * - Module not initialized
         */
        Result start();

        /**
         * Stop audio processing.
         *
         * Transitions back to Initialized state.
         *
         * @return Result indicating success or error.
         */
        Result stop();

        /**
         * Process an audio block (RT-safe entry point).
         *
         * Called from JACK's RT callback thread. Performs:
         * - Pre-DSP metering
         * - Gain/mute processing (future)
         * - Copy to owned buffers
         * - Post-DSP metering (future)
         *
         * @param view Borrowed view of JACK buffers.
         *
         * #### RT Safety
         *
         * - No allocations
         * - No locks
         * - No blocking calls
         * - Safe to call from JACK callback
         *
         * #### Preconditions
         *
         * - Module must be in Active state
         * - view must be valid (view.is_valid() == true)
         *
         */
        void process_block(const AudioBlockView& view);

        /**
         * Get current module state.
         *
         * @return Current state (Uninitialized, Initialized, or Active).
         */
        AudioModuleState get_state() const { return state_; }

        /**
         * Check if module is ready for audio processing.
         *
         * @return True if state is Active.
         */
        bool is_ready() const { return state_ == AudioModuleState::Active; }

    private:
        std::shared_ptr<log::Logger> logger_;
        AudioModuleState state_{AudioModuleState::Uninitialized};

        // Settings cache
        settings::AppSettings settings_;

        // Owned buffers (for non-RT access)
        AudioBuffer buffer_;

        // Future: Pre/post meter atomics
        // Future: Per-channel FIFOs
        // Future: Gain/mute atomics
    };

}

#endif //AKNET_AUDIO_MODULE_H