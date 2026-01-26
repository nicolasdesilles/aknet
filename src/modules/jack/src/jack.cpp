//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "jack.h"

// Note: will need plateform specific includes later when adding Windows support
#include "posix_process_runner.h"

#include "libjack_client_api.h"

namespace aknet::jack {

    std::shared_ptr<IProcessRunner> create_process_runner(
        std::shared_ptr<log::Logger> logger)
    {
        return std::make_shared<PosixProcessRunner>(std::move(logger));
    }

    std::shared_ptr<IJackClientAPI> create_libjack_client_api(
        std::shared_ptr<log::Logger> logger)
    {
        return std::make_shared<LibJackClientAPI>(std::move(logger));
    }

    std::shared_ptr<IAudioDeviceManager> create_device_manager(
    std::shared_ptr<log::Logger> logger)
    {
        return create_audio_device_manager(logger);
    }

} // namespace aknet::jack