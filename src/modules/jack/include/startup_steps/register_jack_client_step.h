//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_REGISTER_JACK_CLIENT_STEP_H
#define AKNET_REGISTER_JACK_CLIENT_STEP_H

#pragma once

#include <startup_step.h>

namespace aknet::jack {

    /**
     * Startup step: Register JACK client and activate audio processing.
     *
     * This step:
     * 1. Ensures JACK server is running with correct settings
     * 2. Opens JACK client
     * 3. Registers input ports
     * 4. Sets up audio processor
     * 5. Activates client (starts real-time audio)
     *
     * #### Success Criteria
     *
     * - JACK server is running
     * - Client is registered with JACK
     * - Input ports are created
     * - Audio processing is active
     *
     * #### Failure Handling
     *
     * - Returns Failed if server cannot be started
     * - Returns Failed if client cannot register
     * - Returns Failed if ports cannot be created
     */
    class RegisterJackClientStep : public startup::IStartupStep {
        startup::StepConfig config_{
            .id = "register_jack_client",
            .display_name = "Register JACK Client",
            .timeout = std::chrono::seconds{10},
            .critical = true,
            .can_skip = false
        };

    public:
        const startup::StepConfig& config() const override {
            return config_;
        }

        startup::StepResult run(startup::StepContext& ctx) override;
    };

} // namespace aknet::jack

#endif //AKNET_REGISTER_JACK_CLIENT_STEP_H