//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "startup_steps/check_jack_installation_step.h"
#include <filesystem>

#include "logger.h"
#include "settings.h"

namespace aknet::jack {

    namespace fs = std::filesystem;

    startup::StepResult CheckJackInstallationStep::run(startup::StepContext& ctx) {
        ctx.logger->info("Checking JACK installation...");

        // Get JACK executable path from settings
        auto snapshot = ctx.settings->snapshot();
        auto jack_path_config = snapshot->jack.server_executable_path;

        if (jack_path_config.empty()) {
            ctx.logger->error("JACK server path not configured");
            return {
                startup::StepStatus::Failed,
                "JACK server path is empty. Please configure in settings."
            };
        }

        ctx.logger->debug("Looking for jackd at: {}", jack_path_config);

        // Resolve actual executable path
        fs::path jack_path(jack_path_config);

        // If path is a directory, look for 'jackd' inside it
        if (fs::exists(jack_path) && fs::is_directory(jack_path)) {
            ctx.logger->debug("Path is a directory, looking for 'jackd' inside");
            jack_path = jack_path / "jackd";
        }

        // Check if executable file exists
        if (!fs::exists(jack_path)) {
            ctx.logger->error("jackd not found at: {}", jack_path.string());
            return {
                startup::StepStatus::Failed,
                "JACK not found at " + jack_path.string() + ". Please install JACK or update path in settings."
            };
        }

        // Verify it's a regular file (not a directory)
        if (!fs::is_regular_file(jack_path)) {
            ctx.logger->error("JACK path is not a regular file: {}", jack_path.string());
            return {
                startup::StepStatus::Failed,
                "JACK path is not a file: " + jack_path.string()
            };
        }

        // Check if executable
        auto perms = fs::status(jack_path).permissions();
        bool is_executable = (perms & fs::perms::owner_exec) != fs::perms::none;

        if (!is_executable) {
            ctx.logger->error("jackd exists but is not executable: {}", jack_path.string());
            return {
                startup::StepStatus::Failed,
                "JACK found but not executable: " + jack_path.string()
            };
        }

        ctx.logger->info("JACK installation verified: {}", jack_path.string());

        return {
            startup::StepStatus::Success,
            "JACK found at " + jack_path.string()
        };
    }

} // namespace aknet::jack