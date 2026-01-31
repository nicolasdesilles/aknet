//
// Created by Nicolas Désilles on 31/01/2026.
//

#include "audio.h"

namespace aknet::audio {

    std::unique_ptr<AudioModule> create_audio_module(
    std::shared_ptr<log::Logger> logger)
    {
        return std::make_unique<AudioModule>(std::move(logger));
    }

} // namespace aknet::audio