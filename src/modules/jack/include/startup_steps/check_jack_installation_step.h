//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_CHECK_JACK_INSTALLATION_STEP_H
#define AKNET_CHECK_JACK_INSTALLATION_STEP_H

#include <startup_step.h>

namespace aknet::jack {

    /**
     * Startup step: Check if JACK is installed.
     *
     * Verifies that the jackd executable exists at the configured path.
     *
     * #### Success Criteria
     *
     * - `jack.server_executable_path` is not empty
     * - File exists at that path
     * - File is executable
     *
     * #### Failure Handling
     *
     * - Returns Failed status if jackd not found
     * - User must install JACK or update settings
     */
    class CheckJackInstallationStep : public startup::IStartupStep {
        startup::StepConfig config_{
            .id = "check_jack_installation",
            .display_name = "Check JACK Installation",
            .timeout = std::chrono::seconds{5},
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

#endif //AKNET_CHECK_JACK_INSTALLATION_STEP_H