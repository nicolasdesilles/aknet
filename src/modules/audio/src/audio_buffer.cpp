//
// Created by Nicolas Désilles on 31/01/2026.
//

#include "audio_buffer.h"
#include <algorithm>
#include <cstring>

namespace aknet::audio {

    AudioBuffer::AudioBuffer(uint32_t nframes, uint32_t channels) {
        resize(nframes, channels);
    }

    void AudioBuffer::resize(uint32_t nframes, uint32_t channels) {
        nframes_ = nframes;
        num_channels_ = channels;

        if (nframes == 0 || channels == 0) {
            data_.clear();
            channel_ptrs_.clear();
            return;
        }

        // Allocate planar storage
        data_.resize(nframes * channels, 0.0f);
        channel_ptrs_.resize(channels);

        update_channel_pointers();
    }

    void AudioBuffer::update_channel_pointers() {
        for (uint32_t ch = 0; ch < num_channels_; ++ch) {
            channel_ptrs_[ch] = data_.data() + (ch * nframes_);
        }
    }

    float* AudioBuffer::channel(uint32_t channel) {
        if (channel >= num_channels_) {
            return nullptr;
        }
        return channel_ptrs_[channel];
    }

    const float* AudioBuffer::channel(uint32_t channel) const {
        if (channel >= num_channels_) {
            return nullptr;
        }
        return channel_ptrs_[channel];
    }

    void AudioBuffer::clear() {
        std::fill(data_.begin(), data_.end(), 0.0f);
    }

} // namespace aknet::audio