//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_JACK_AUDIO_PROCESSOR_H
#define AKNET_JACK_AUDIO_PROCESSOR_H

#pragma once

#include <vector>
#include <atomic>

namespace aknet::jack {

    /**
     * Meter reading for a single channel.
     */
    struct ChannelMeter {
        float rms_db = -std::numeric_limits<float>::infinity();   ///< RMS level in dB
        float peak_db = -std::numeric_limits<float>::infinity();  ///< Peak level in dB
    };

    /**
     * Real-time audio processor for JACK.
     *
     * Processes audio in JACK's real-time thread and computes level meters.
     * Thread-safe: process() called from RT thread, getters called from UI thread.
     *
     * ## Level Meter Algorithm
     *
     * **RMS (Root Mean Square)**: Measures average signal energy
     * - Computed per process cycle (e.g., 256 samples at 48kHz = ~5ms)
     * - Formula: sqrt(sum(sample^2) / num_samples)
     * - Converted to dB: 20 * log10(rms_linear)
     * - 0 dBFS = full scale, -inf dB = silence
     *
     * **Peak Hold**: Tracks maximum sample value with decay
     * - Finds max absolute sample value in current cycle
     * - Decays exponentially if current cycle is quieter
     * - Converted to dB: 20 * log10(peak_linear)
     *
     * ## Thread Safety
     *
     * Uses `std::atomic<float>` with relaxed memory ordering:
     * - RT thread writes levels after each process cycle
     * - UI thread reads levels at ~20-60Hz for visualization
     * - No locks or allocations in RT thread
     *
     * @see https://jackaudio.org/api/group__ServerAPI.html
     */
    class JackAudioProcessor {
    public:
        /**
         * Construct processor for a given number of channels.
         *
         * @param num_channels Number of input channels to meter.
         */
        explicit JackAudioProcessor(int num_channels);

        ~JackAudioProcessor() = default;

        // Non-copyable, moveable
        JackAudioProcessor(const JackAudioProcessor&) = delete;
        JackAudioProcessor& operator=(const JackAudioProcessor&) = delete;
        JackAudioProcessor(JackAudioProcessor&&) = default;
        JackAudioProcessor& operator=(JackAudioProcessor&&) = default;

        /**
         * Process audio samples and update meters.
         *
         * Called from JACK's real-time thread. Must not block, allocate, or lock.
         *
         * @param num_samples Number of samples to process (JACK's nframes).
         * @param input_buffers Array of pointers to input buffers (one per channel). Each buffer contains num_samples of float audio data.
         */
        void process(uint32_t num_samples, const float* const* input_buffers);

        /**
         * Get current meter readings for all channels.
         *
         * Thread-safe: can be called from any thread (typically UI thread).
         *
         * @return Vector of ChannelMeter, one per channel.
         */
        std::vector<ChannelMeter> get_meters() const;

        /**
         * Reset peak hold values to -inf.
         *
         * Thread-safe: can be called from any thread.
         */
        void reset_peaks();

        /**
         * Get number of channels this processor handles.
         *
         * @return Channel count.
         */
        int get_channel_count() const { return num_channels_; }

    private:
        int num_channels_;

        // Atomic storage for thread-safe communication
        // RT thread writes, UI thread reads
        std::vector<std::atomic<float>> rms_db_;
        std::vector<std::atomic<float>> peak_db_;

        // Peak decay factor per sample (applied in RT thread)
        // Adjust this to control how fast peaks fall
        static constexpr float PEAK_DECAY_PER_SAMPLE = 0.9999f;

        /**
         * Compute RMS in linear scale.
         */
        static float compute_rms_linear(const float* buffer, uint32_t num_samples);

        /**
         * Find peak absolute value in linear scale.
         */
        static float compute_peak_linear(const float* buffer, uint32_t num_samples);

        /**
         * Convert linear amplitude to dB.
         *
         * @param linear_value Linear amplitude (0.0 to 1.0+).
         * @return dB value, or -inf if linear_value is 0.
         */
        static float linear_to_db(float linear_value);
    };

} // namespace aknet::jack

#endif //AKNET_JACK_AUDIO_PROCESSOR_H