//
// Created by Nicolas Désilles on 06/01/2026.
//

#ifndef AKNET_STARTUP_ENGINE_H
#define AKNET_STARTUP_ENGINE_H

#pragma once


#include <atomic>
#include <memory>
#include <vector>

#include "logger.h"
#include "startup.h"
#include "startup_step.h"
#include "clock.h"

namespace aknet::settings {
    class Settings;
}

namespace aknet::startup {

    class StartupEngine final {
    public:

        using StepPtr = std::unique_ptr<IStartupStep>;

        struct RunOptions {
            bool reset_progress_before_run = true;
        };

        // Constructor
        StartupEngine(std::shared_ptr<log::Logger> logger,
                      std::shared_ptr<settings::Settings> settings,
                      std::shared_ptr<IClock> clock = std::make_shared<SteadyClock>());

        // Destructor
        ~StartupEngine() = default;

        // Non-copyable
        StartupEngine(const StartupEngine&) = delete;
        StartupEngine& operator=(const StartupEngine&) = delete;

        // Step management

        // validates config + uniqueness; builds initial progress
        Result set_steps(std::vector<StepPtr> steps);

        // validates and appends
        Result add_step(StepPtr step);

        // clears steps + progress
        void clear_steps();

        // Execution

        // Synchronous run: executes steps in order and returns the final progress snapshot.
        const SequenceProgress& run(const RunOptions& options);

        // cancellation: runner checks between steps; steps can check via StepContext.
        void request_abort();
        void reset_abort();

        // Accessors
        const SequenceProgress& progress() const;
        AppState state() const;
        bool has_steps() const;
        std::size_t step_count() const;

    private:
        // Build progress_.steps from current steps_ configs.
        Result rebuild_progress_snapshot_(AppState state);

        // Execute one step and update progress_.
        void run_step_(std::size_t index);

        // Terminal handling
        void transition_to_off_with_error_(std::string error, bool can_retry);

        // Dependencies owned by core; stored here for step execution.
        std::shared_ptr<log::Logger> logger_;
        std::shared_ptr<settings::Settings> settings_;
        std::shared_ptr<IClock> clock_;

        std::atomic_bool abort_requested_{false};

        std::vector<StepPtr> steps_;
        SequenceProgress progress_{};
    };

} // namespace aknet::startup

#endif //AKNET_STARTUP_ENGINE_H