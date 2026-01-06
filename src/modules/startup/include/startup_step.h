//
// Created by Nicolas Désilles on 06/01/2026.
//

#ifndef AKNET_STARTUP_STEP_H
#define AKNET_STARTUP_STEP_H

#pragma once
#include <atomic>
#include <memory>

#include "startup.h"
#include "clock.h"

namespace aknet::log {
    class Logger;
}

namespace aknet::settings {
    class Settings;
}

namespace aknet::startup {

    struct StepResult {
        StepStatus status{StepStatus::Success};
        std::string message;
    };

    struct StepContext {
        // Owned in the Startup engine
        std::atomic_bool* abort_flag{nullptr};
        const IClock* clock{nullptr};
        std::chrono::steady_clock::time_point deadline{};

        std::shared_ptr<log::Logger> logger;
        std::shared_ptr<settings::Settings>* settings{nullptr};

        bool abort_requested() const {
            return abort_flag && abort_flag->load(std::memory_order_relaxed);
        }

        bool is_expired() const {
            return clock && (clock->now() >= deadline);
        }
    };

    class IStartupStep {
    public:
        virtual ~IStartupStep() = default;

        virtual const StepConfig& config() const = 0;

        // Expected to return Success/Failed as a base.
        // The startup engine may override with TimedOut/Aborted based on its own checks.
        virtual StepResult run(StepContext& context) = 0;
    };

} // namespace aknet::startup

#endif // AKNET_STARTUP_STEP_H