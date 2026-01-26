//
// Created by Nicolas Désilles on 25/01/2026.
//

#include <startup_steps/ensure_jack_server_step.h>
#include <jack_module.h>

namespace aknet::jack {

    startup::StepResult EnsureJackServerStep::run(startup::StepContext& ctx) {
        ctx.logger->info("Initializing JACK module...");

        if (!ctx.jack_module) {
            ctx.logger->error("JackModule not available in context");
            return {
                startup::StepStatus::Failed,
                "JACK module not initialized"
            };
        }

        // Get current settings snapshot
        auto snapshot = ctx.settings->snapshot();

        // Initialize JACK module with settings
        // This creates the server manager, client, and audio processor
        auto result = ctx.jack_module->init(*snapshot);

        if (!result.ok) {
            ctx.logger->error("Failed to initialize JACK module: {}", result.error);
            return {
                startup::StepStatus::Failed,
                result.error
            };
        }

        ctx.logger->info("JACK module initialized successfully");

        return {
            startup::StepStatus::Success,
            "JACK module ready"
        };
    }

}