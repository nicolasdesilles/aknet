//
// Created by Nicolas Désilles on 31/01/2026.
//

#include "audio_module.h"
#include <stdexcept>

namespace aknet::audio {

    AudioModule::AudioModule(std::shared_ptr<log::Logger> logger)
        : logger_(std::move(logger))
    {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
    }

    Result AudioModule::init(const settings::AppSettings& settings) {
        if (state_ != AudioModuleState::Uninitialized) {
            logger_->error("AudioModule already initialized");
            return {false, "Module already initialized"};
        }

        // Validate settings
        if (settings.audio.num_channels < 1) {
            logger_->error("Invalid num_channels: {}", settings.audio.num_channels);
            return {false, "num_channels must be >= 1"};
        }

        if (settings.audio.buffer_size < 1) {
            logger_->error("Invalid buffer_size: {}", settings.audio.buffer_size);
            return {false, "buffer_size must be >= 1"};
        }

        logger_->info("Initializing Audio module");
        logger_->info("  Channels: {}", settings.audio.num_channels);
        logger_->info("  Buffer size: {} samples", settings.audio.buffer_size);

        settings_ = settings;

        // Preallocate owned buffer
        buffer_.resize(settings.audio.buffer_size, settings.audio.num_channels);

        state_ = AudioModuleState::Initialized;
        logger_->info("Audio module initialized successfully");

        return {true, ""};
    }

    Result AudioModule::start() {
        if (state_ == AudioModuleState::Uninitialized) {
            logger_->error("Cannot start: module not initialized");
            return {false, "Module not initialized"};
        }

        if (state_ == AudioModuleState::Active) {
            logger_->warn("Module already active");
            return {true, ""};  // Already started, not an error
        }

        logger_->info("Starting Audio module");

        state_ = AudioModuleState::Active;
        logger_->info("Audio module started successfully");

        return {true, ""};
    }

    Result AudioModule::stop() {
        if (state_ != AudioModuleState::Active) {
            logger_->debug("Module not active, nothing to stop");
            return {true, ""};
        }

        logger_->info("Stopping Audio module");

        state_ = AudioModuleState::Initialized;
        logger_->info("Audio module stopped");

        return {true, ""};
    }

    void AudioModule::process_block(const AudioBlockView& view) {

        // TODO
        // Stub for now
        // Later:
        // - Pre-DSP metering
        // - Gain/mute processing
        // - Copy to owned buffer
        // - Post-DSP metering
        // - Write to FIFOs

        if (state_ != AudioModuleState::Active) {
            return; // Not active, skip processing
        }

        if (!view.is_valid()) {
            logger_->warn("Invalid AudioBlockView passed to process_block");
            return;
        }

        // TODO: Add audio processing logic
    }

} // namespace aknet::audio