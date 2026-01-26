//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_ENSURE_JACK_SERVER_STEP_H
#define AKNET_ENSURE_JACK_SERVER_STEP_H

#pragma once

#include <startup_step.h>

namespace aknet::jack {

    /**
     * Startup step: Initialize JACK module with settings.
     *
     * This step initializes the JackModule facade, which sets up:
     * - JackServerManager for server lifecycle
     * - JackClient for client registration
     * - JackAudioProcessor for level metering
     *
     * Does NOT start the server or client yet (that's RegisterJackClientStep).
     *
     * #### Success Criteria
     *
     * - JackModule is successfully initialized
     * - Settings are valid (num_channels >= 1, client_name not empty)
     *
     * #### Failure Handling
     *
     * - Returns Failed if initialization fails
     */
    class EnsureJackServerStep : public startup::IStartupStep {
        startup::StepConfig config_{
            .id = "ensure_jack_server",
            .display_name = "Initialize JACK Module",
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

#endif //AKNET_ENSURE_JACK_SERVER_STEP_H