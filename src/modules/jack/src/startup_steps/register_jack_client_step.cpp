//
// Created by Nicolas Désilles on 25/01/2026.
//

#include <startup_steps/register_jack_client_step.h>
#include <jack_module.h>

namespace aknet::jack {

    startup::StepResult RegisterJackClientStep::run(startup::StepContext& ctx) {
        ctx.logger->info("Starting JACK client...");

        if (!ctx.jack_module) {
            ctx.logger->error("JackModule not available in context");
            return {
                startup::StepStatus::Failed,
                "JACK module not initialized"
            };
        }

        // Start the JACK module (starts server if needed, registers client, activates)
        auto result = ctx.jack_module->start();

        if (!result.ok) {
            ctx.logger->error("Failed to start JACK client: {}", result.error);
            return {
                startup::StepStatus::Failed,
                result.error
            };
        }

        ctx.logger->info("JACK client registered and active");

        return {
            startup::StepStatus::Success,
            "JACK client active"
        };
    }

}