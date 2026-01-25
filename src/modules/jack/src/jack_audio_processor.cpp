//
// Created by Nicolas Désilles on 25/01/2026.
//

#include <jack_audio_processor.h>
#include <cmath>
#include <limits>
#include <algorithm>

namespace aknet::jack {

JackAudioProcessor::JackAudioProcessor(int num_channels):
    num_channels_(num_channels),
    rms_db_(num_channels),
    peak_db_(num_channels)
{
    // Initialize to -inf dB
    const float neg_inf = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < num_channels_; ++i) {
        rms_db_[i].store(neg_inf, std::memory_order_relaxed);
        peak_db_[i].store(neg_inf, std::memory_order_relaxed);
    }
}

void JackAudioProcessor::process(uint32_t num_samples, const float* const* input_buffers) {
    for (int ch = 0; ch < num_channels_; ++ch) {
        const float* buffer = input_buffers[ch];

        // Compute RMS
        float rms_linear = compute_rms_linear(buffer, num_samples);
        float rms_db = linear_to_db(rms_linear);
        rms_db_[ch].store(rms_db, std::memory_order_relaxed);

        // Compute peak with decay
        float current_peak_linear = compute_peak_linear(buffer, num_samples);

        // Get previous peak (in linear scale for decay calculation)
        float prev_peak_db = peak_db_[ch].load(std::memory_order_relaxed);
        float prev_peak_linear = std::isinf(prev_peak_db) ? 0.0f : std::pow(10.0f, prev_peak_db / 20.0f);

        // Apply decay: peak decays exponentially per sample
        float decay_factor = std::pow(PEAK_DECAY_PER_SAMPLE, static_cast<float>(num_samples));
        float decayed_peak_linear = prev_peak_linear * decay_factor;

        // New peak is max of current sample peak and decayed previous peak
        float new_peak_linear = std::max(current_peak_linear, decayed_peak_linear);
        float new_peak_db = linear_to_db(new_peak_linear);

        peak_db_[ch].store(new_peak_db, std::memory_order_relaxed);
    }
}

std::vector<ChannelMeter> JackAudioProcessor::get_meters() const {
    std::vector<ChannelMeter> meters(num_channels_);
    for (int ch = 0; ch < num_channels_; ++ch) {
        meters[ch].rms_db = rms_db_[ch].load(std::memory_order_relaxed);
        meters[ch].peak_db = peak_db_[ch].load(std::memory_order_relaxed);
    }
    return meters;
}

void JackAudioProcessor::reset_peaks() {
    const float neg_inf = -std::numeric_limits<float>::infinity();
    for (int ch = 0; ch < num_channels_; ++ch) {
        peak_db_[ch].store(neg_inf, std::memory_order_relaxed);
    }
}

float JackAudioProcessor::compute_rms_linear(const float* buffer, uint32_t num_samples) {
    if (num_samples == 0) {
        return 0.0f;
    }

    float sum_squares = 0.0f;
    for (uint32_t i = 0; i < num_samples; ++i) {
        float sample = buffer[i];
        sum_squares += sample * sample;
    }

    return std::sqrt(sum_squares / static_cast<float>(num_samples));
}

float JackAudioProcessor::compute_peak_linear(const float* buffer, uint32_t num_samples) {
    float peak = 0.0f;
    for (uint32_t i = 0; i < num_samples; ++i) {
        peak = std::max(peak, std::abs(buffer[i]));
    }
    return peak;
}

float JackAudioProcessor::linear_to_db(float linear_value) {
    // Avoid log(0) which is -inf
    if (linear_value <= 0.0f) {
        return -std::numeric_limits<float>::infinity();
    }
    // 20 * log10(x) formula for amplitude to dB
    return 20.0f * std::log10(linear_value);
}

} // namespace aknet::jack