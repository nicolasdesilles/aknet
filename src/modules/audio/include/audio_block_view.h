//
// Created by Nicolas Désilles on 31/01/2026.
//

#ifndef AKNET_AUDIO_BLOCK_VIEW_H
#define AKNET_AUDIO_BLOCK_VIEW_H

#pragma once

#include <cstdint>

namespace aknet::audio {
    /**
    * RT-safe view of borrowed audio buffers from JACK.
    *
    * This is a lightweight, non-owning view that wraps JACK's audio buffers
    * during the process callback. It provides a clean interface to audio data
    * without allocations or ownership.
    *
    * #### RT Safety
    *
    * - No allocations (buffers pointer is borrowed)
    * - No locks
    * - Trivially copyable
    * - Only valid for the duration of the JACK callback
    *
    * #### Memory Layout
    *
    * Buffers are planar (channel-major):
    * - `buffers[0]` points to channel 0 samples
    * - `buffers[1]` points to channel 1 samples
    * - etc.
    *
    * Each buffer contains `nframes` samples.
    *
    */
    struct AudioBlockView {

        uint32_t nframes{0}; ///< Number of frames in this block
        uint32_t channels{0}; ///< Number of channels
        const float* const* buffers{nullptr}; ///< Planar buffers (borrowed, not owned)

        /**
        * Check if this view is valid.
        *
        * @return True if nframes > 0, channels > 0, and buffers is not null.
        */
        bool is_valid() const {
            return nframes > 0 && channels > 0 && buffers != nullptr;
        }
    };
}

#endif //AKNET_AUDIO_BLOCK_VIEW_H