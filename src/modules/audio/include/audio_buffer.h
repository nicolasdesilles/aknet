//
// Created by Nicolas Désilles on 31/01/2026.
//

#ifndef AKNET_AUDIO_BUFFER_H
#define AKNET_AUDIO_BUFFER_H

#pragma once
#include <vector>
#include <cstdint>
namespace aknet::audio {
    /**
    * Owned planar audio buffer storage.
    *
    * Provides preallocated storage for audio data that needs to outlive the JACK callback.
    * Used for any non-RT thread that needs audio data
    *
    * #### Memory Layout
    *
    * Planar (channel-major) storage:
    * - All channel 0 samples are contiguous
    * - Followed by all channel 1 samples
    * - etc.
    *
    * #### Allocation Strategy
    *
    * - Preallocate during module initialization
    * - Resize only when audio starts/stops (not in RT)
    * - No allocations during process()
    *
    * #### Thread Safety
    *
    * NOT thread-safe. External synchronization required if accessed from multiple threads.
    */
    class AudioBuffer {
    public:
        /**
        * Construct an empty buffer.
        */
        AudioBuffer() = default;

        /**
        * Construct with preallocated size.
        *
        * @param nframes Number of frames per channel.
        * @param channels Number of channels.
        */
        AudioBuffer(uint32_t nframes, uint32_t channels);

        /**
        * Resize the buffer.
        *
        * Allocates new storage. Old data is discarded.
        * Safe to call from non-RT threads only.
        *
        * @param nframes Number of frames per channel.
        * @param channels Number of channels.
        */
        void resize(uint32_t nframes, uint32_t channels);

        /**
        * Get pointer to a specific channel's data.
        *
        * @param channel Channel index (0-based).
        * @return Pointer to channel data, or nullptr if invalid channel.
        */
        float* channel(uint32_t channel);
        const float* channel(uint32_t channel) const;

        /**
        * Get array of channel pointers (for planar access).
        *
        * @return Pointer to array of channel pointers.
        */
        float* const* channels() { return channel_ptrs_.data(); }
        const float* const* channels() const { return channel_ptrs_.data(); }

        /**
        * Get number of frames.
        */
        uint32_t nframes() const { return nframes_; }

        /**
        * Get number of channels.
        */
        uint32_t num_channels() const { return num_channels_; }

        /**
        * Clear all samples to zero.
        */
        void clear();

        /**
        * Check if buffer is allocated.
        *
        * @return True if nframes > 0 and channels > 0.
        */
        bool is_allocated() const {
            return nframes_ > 0 && num_channels_ > 0;
        }

    private:
        uint32_t nframes_{0};
        uint32_t num_channels_{0};

        // Planar storage: all samples for all channels
        std::vector<float> data_;

        // Pointers to each channel's start in data_
        std::vector<float*> channel_ptrs_;
        void update_channel_pointers();
    };


}

#endif //AKNET_AUDIO_BUFFER_H