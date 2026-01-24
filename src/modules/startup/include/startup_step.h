//
// startup_step.h - Step interface and execution context for startup steps.
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

    /**
     * Step interface and execution context for implementing startup steps.
     */
    namespace steps {

        /**
         * Result returned by a step's run() method.
         *
         * Contains the final status of the step and an optional message.
         * The engine may override the status (e.g., to TimedOut) based on its own checks.
         */
        struct StepResult {
            StepStatus status{StepStatus::Success};  ///< Final status of the step
            std::string message;                      ///< Optional status message or error description
        };

        /**
         * Execution context provided to steps during run().
         *
         * Contains everything a step needs to execute: abort checking, timeout checking, logging, and settings access.
         * Created by the engine before calling step.run().
         *
         * #### Thread Safety
         *
         * The abort_flag is atomic and can be checked safely.
         * Logger is thread-safe.
         * Settings should only be read, not modified, during step execution.
         *
         * #### Example
         *
         * ```cpp
         * StepResult run(StepContext& ctx) override {
         *     for (int i = 0; i < 100; ++i) {
         *         // Check for abort request periodically
         *         if (ctx.abort_requested()) {
         *             return {StepStatus::Aborted, "User cancelled"};
         *         }
         *
         *         // Check for timeout
         *         if (ctx.is_expired()) {
         *             return {StepStatus::TimedOut, "Operation took too long"};
         *         }
         *
         *         ctx.logger->debug("Processing item {}", i);
         *         do_work();
         *     }
         *     return {StepStatus::Success, "Completed"};
         * }
         * ```
         */
        struct StepContext {
            /**
             * Pointer to the engine's abort flag.
             * Check via abort_requested() helper method.
             */
            std::atomic_bool* abort_flag{nullptr};

            /**
             * Clock for time-based operations.
             * Used internally for deadline checking.
             */
            const IClock* clock{nullptr};

            /**
             * Deadline for this step's execution.
             * If clock->now() >= deadline, the step has timed out.
             */
            std::chrono::steady_clock::time_point deadline{};

            /**
             * Logger for diagnostic output within the step.
             */
            std::shared_ptr<log::Logger> logger;

            /**
             * Settings access (read-only during step execution).
             */
            settings::Settings* settings{nullptr};

            /**
             * Check if abort has been requested.
             *
             * Call this periodically in long-running operations to allow responsive cancellation.
             *
             * @return True if abort was requested.
             */
            bool abort_requested() const {
                return abort_flag && abort_flag->load(std::memory_order_relaxed);
            }

            /**
             * Check if the step's deadline has passed.
             *
             * @return True if current time >= deadline.
             */
            bool is_expired() const {
                return clock && (clock->now() >= deadline);
            }

            /**
             * Throw an exception if abort was requested.
             *
             * Alternative to checking abort_requested() - useful for exception-based abort handling.
             *
             * #### Exceptions
             *
             * - Throws `std::runtime_error` with message "Step aborted" if abort was requested.
             *
             * #### Example
             *
             * ```cpp
             * try {
             *     for (int i = 0; i < 100; ++i) {
             *         ctx.check_abort_point();
             *         do_work();
             *     }
             * } catch (const std::runtime_error& e) {
             *     return {StepStatus::Aborted, e.what()};
             * }
             * ```
             */
            void check_abort_point() const {
                if (abort_requested()) {
                    throw std::runtime_error("Step aborted");
                }
            }
        };

        /**
         * Abstract interface for startup steps.
         *
         * All startup steps must implement this interface.
         * Steps are executed sequentially by the StartupEngine.
         *
         * #### Implementing a Custom Step
         *
         * 1. Inherit from IStartupStep
         * 2. Store a StepConfig in your class
         * 3. Implement config() to return your configuration
         * 4. Implement run() with your step's logic
         *
         * #### Example
         *
         * ```cpp
         * class CheckJackInstallationStep : public IStartupStep {
         *     StepConfig config_{
         *         .id = "check_jack",
         *         .display_name = "Check JACK Installation",
         *         .timeout = std::chrono::seconds{5},
         *         .critical = true,
         *         .can_skip = false
         *     };
         *
         * public:
         *     const StepConfig& config() const override {
         *         return config_;
         *     }
         *
         *     StepResult run(StepContext& ctx) override {
         *         ctx.logger->info("Checking JACK installation...");
         *
         *         if (!is_jack_installed()) {
         *             return {StepStatus::Failed, "JACK is not installed"};
         *         }
         *
         *         return {StepStatus::Success, "JACK found"};
         *     }
         * };
         * ```
         */
        class IStartupStep {
        public:
            virtual ~IStartupStep() = default;

            /**
             * Get the step's configuration.
             *
             * @return Reference to the step's StepConfig.
             */
            virtual const StepConfig& config() const = 0;

            /**
             * Execute the step.
             *
             * Performs the step's work and returns the result.
             * The engine may override the returned status based on timeout or abort checks.
             *
             * @param context Execution context with abort flag, deadline, logger, and settings.
             *
             * @return StepResult with status and message.
             *
             * @note
             * Steps should check abort_requested() periodically in long-running operations.
             * The engine checks the deadline after run() returns, but steps can check is_expired() for early exit.
             */
            virtual StepResult run(StepContext& context) = 0;
        };

    } // namespace steps

    // Re-export at startup:: level for API convenience
    using steps::StepResult;
    using steps::StepContext;
    using steps::IStartupStep;

} // namespace aknet::startup

#endif // AKNET_STARTUP_STEP_H
