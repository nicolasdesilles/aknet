//
// steps_definition.h - Factory function for creating default startup steps.
//

#ifndef AKNET_STEPS_DEFINITION_H
#define AKNET_STEPS_DEFINITION_H

#pragma once

#include <startup_step.h>
#include <startup_steps/check_jack_installation_step.h>
#include <startup_steps/ensure_jack_server_step.h>
#include <startup_steps/register_jack_client_step.h>
#include <memory>
#include <vector>

namespace aknet::startup {

    /**
     * Create the default startup step sequence.
     *
     * @return Vector of startup steps ready to be passed to StartupManager::set_steps().
     */
    inline std::vector<std::unique_ptr<IStartupStep>> create_default_steps() {
        std::vector<std::unique_ptr<IStartupStep>> steps;

        steps.push_back(std::make_unique<jack::CheckJackInstallationStep>());
        steps.push_back(std::make_unique<jack::EnsureJackServerStep>());
        steps.push_back(std::make_unique<jack::RegisterJackClientStep>());

        return steps;
    }

} // namespace aknet::startup

#endif // AKNET_STEPS_DEFINITION_H