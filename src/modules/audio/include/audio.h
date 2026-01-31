//
// Created by Nicolas Désilles on 31/01/2026.
//

#ifndef AKNET_AUDIO_H
#define AKNET_AUDIO_H

#include "audio_module.h"
#include <memory>

namespace aknet::audio {

    /**
    * Factory function to create an AudioModule.
    *
    * @param logger Logger instance.
    * @return Unique pointer to AudioModule.
    *
    * #### Example
    *
    * ```cpp
    * auto audio_module = audio::create_audio_module(logger);
    * audio_module->init(settings);
    * audio_module->start();
    * ```
    */
    std::unique_ptr<AudioModule> create_audio_module(
    std::shared_ptr<log::Logger> logger
    );

}


#endif //AKNET_AUDIO_H