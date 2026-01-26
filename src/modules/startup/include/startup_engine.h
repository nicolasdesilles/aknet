//
// startup_engine.h - Synchronous startup sequence execution engine.
//

#ifndef AKNET_STARTUP_ENGINE_H
#define AKNET_STARTUP_ENGINE_H

#pragma once


#include <atomic>
#include <memory>
#include <vector>
#include <stdexcept>

#include <logger.h>
#include "startup.h"
#include "startup_step.h"
#include "clock.h"

namespace aknet::settings {
    class Settings;
}

namespace aknet::jack {
    class JackModule;
}

namespace aknet::startup {

    /**
     * Synchronous startup sequence execution engine.
     */
    namespace engine {

    /**
     * Synchronous startup sequence execution engine.
     *
     * Executes registered steps in order, manages timeouts and abort handling, and tracks progress.
     * Designed to be used by StartupManager, not directly by application code.
     *
     * @warning
     * StartupEngine is NOT thread-safe.
     * Use StartupManager for thread-safe async execution.
     *
     * #### Execution Flow
     *
     * 1. Steps are registered via set_steps() or add_step()
     * 2. run() executes steps sequentially
     * 3. For each step:
     *    - Check abort flag (if set, mark remaining as Aborted, return)
     *    - Execute step with StepContext
     *    - Check timeout/abort after execution
     *    - If critical step failed/timed out/aborted, stop and set can_retry=true
     * 4. If all steps complete, state transitions to Active
     *
     * #### Critical vs Non-Critical Steps
     *
     * - **Critical steps**: Failure stops the entire sequence, sets can_retry=true
     * - **Non-critical steps**: Failure is logged but sequence continues
     */
    class StartupEngine final {
    public:
        /**
         * Unique pointer to a startup step.
         */
        using StepPtr = std::unique_ptr<IStartupStep>;

        /**
         * Options for run() and retry() execution.
         */
        struct RunOptions {
            /**
             * If true, reset all steps to Pending before running.
             * Set to false to resume from current state.
             */
            bool reset_progress_before_run = true;

            /**
             * Called after each step completes with the full progress snapshot.
             */
            std::function<void(const SequenceProgress&)> progress_callback = nullptr;

            /**
             * Called when a step begins execution.
             *
             * @param index Step index (0-based).
             * @param id Step identifier.
             */
            std::function<void(int, const std::string&)> step_started_callback = nullptr;

            /**
             * Called when a step finishes.
             *
             * @param index Step index (0-based).
             * @param id Step identifier.
             * @param status Final status of the step.
             */
            std::function<void(int, const std::string&, StepStatus)> step_completed_callback = nullptr;
        };

        /**
         * Construct a StartupEngine with dependencies.
         *
         * @param logger Logger for diagnostic output.
         * @param settings Settings access for steps.
         * @param clock Clock for timeout checking (defaults to SteadyClock).
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         * - Throws `std::invalid_argument` if settings is null.
         * - Throws `std::invalid_argument` if clock is null.
         */
        StartupEngine(std::shared_ptr<log::Logger> logger,
                      std::shared_ptr<settings::Settings> settings,
                      std::shared_ptr<IClock> clock = std::make_shared<SteadyClock>());

        /**
         * Destructor.
         */
        ~StartupEngine() = default;

        // Non-copyable
        StartupEngine(const StartupEngine&) = delete;
        StartupEngine& operator=(const StartupEngine&) = delete;

        // -- Step Management --

        /**
         * Replace all registered steps.
         *
         * Validates all configurations and ensures unique IDs.
         * Resets progress to Off state with all steps Pending.
         *
         * @param steps Vector of steps to register (ownership transferred).
         *
         * @return Result indicating success or validation error.
         */
        Result set_steps(std::vector<StepPtr> steps);

        /**
         * Append a step to the existing sequence.
         *
         * Validates the step configuration and ensures ID is unique.
         *
         * @param step Step to add (ownership transferred).
         *
         * @return Result indicating success or validation error.
         */
        Result add_step(StepPtr step);

        /**
         * Remove all registered steps.
         *
         * Resets state to Off with empty progress.
         */
        void clear_steps();

        // -- Execution --

        /**
         * Execute the startup sequence synchronously.
         *
         * Runs all registered steps in order.
         * Returns when sequence completes, fails, or is aborted.
         *
         * @param options Execution options (callbacks, reset behavior).
         *
         * @return Reference to the final progress snapshot.
         *
         * @note
         * This method blocks until the sequence completes.
         * For async execution, use StartupManager::start_async().
         */
        const SequenceProgress& run(const RunOptions& options);

        /**
         * Retry the startup sequence after a failure.
         *
         * Only succeeds if state is Off and can_retry is true (set after critical failure).
         * Resets abort flag and all steps to Pending before re-running.
         *
         * @param options Execution options (callbacks, reset behavior).
         *
         * @return Result indicating whether retry was initiated.
         */
        Result retry(const RunOptions& options = {true, nullptr, nullptr, nullptr});

        /**
         * Request abort of the running sequence.
         *
         * Sets the abort flag which is checked between steps and can be checked by steps via StepContext.
         *
         * @param reason Why the abort is being requested (default: UserRequested).
         */
        void request_abort(AbortReason reason = AbortReason::UserRequested);

        /**
         * Clear the abort flag.
         *
         * Called automatically before retry().
         * Can be called manually if needed.
         */
        void reset_abort();

        // -- Accessors --

        /**
         * Get the current progress snapshot.
         *
         * @return Reference to the SequenceProgress.
         */
        const SequenceProgress& progress() const;

        /**
         * Get the current application state.
         *
         * @return Current AppState (Off, Booting, Active, ShuttingDown).
         */
        AppState state() const;

        /**
         * Check if any steps are registered.
         *
         * @return True if at least one step is registered.
         */
        bool has_steps() const;

        /**
         * Get the number of registered steps.
         *
         * @return Step count.
         */
        std::size_t step_count() const;

        /**
         * Check if retry is allowed.
         *
         * @return True if state is Off and can_retry is true.
         */
        bool can_retry() const;

        /**
        * Set the JackModule for steps to use.
        *
        * @param jack_module Shared pointer to JackModule.
        */
        void set_jack_module(std::shared_ptr<jack::JackModule> jack_module);

    private:
        /**
         * Rebuild progress snapshot from current steps.
         */
        void rebuild_progress_snapshot(AppState state);

        /**
         * Execute a single step and update progress.
         *
         * @return True if sequence should continue, false if stopped.
         */
        bool run_step(std::size_t index);

        /**
         * Transition to Off state with an error message.
         */
        void transition_to_off_with_error(std::string error, bool can_retry);

        /**
         * Logger for diagnostic output.
         */
        std::shared_ptr<log::Logger> logger_;

        /**
         * Settings access for steps.
         */
        std::shared_ptr<settings::Settings> settings_;

        /**
         * Jack Module access for steps.
         */
        std::shared_ptr<jack::JackModule> jack_module_;

        /**
         * Clock for timeout checking.
         */
        std::shared_ptr<IClock> clock_;

        /**
         * Abort request flag (atomic for cross-thread visibility).
         */
        std::atomic_bool abort_requested_{false};

        /**
         * Reason for abort request.
         */
        std::atomic<AbortReason> abort_reason_{AbortReason::None};

        /**
         * Registered steps.
         */
        std::vector<StepPtr> steps_;

        /**
         * Current progress snapshot.
         */
        SequenceProgress progress_{};

        /**
         * Active progress callback.
         */
        std::function<void(const SequenceProgress&)> current_progress_callback_;

        /**
         * Active step started callback.
         */
        std::function<void(int, const std::string&)> current_step_started_callback_;

        /**
         * Active step completed callback.
         */
        std::function<void(int, const std::string&, StepStatus)> current_step_completed_callback_;
    };

    } // namespace engine

    // Re-export at startup:: level for API convenience
    using engine::StartupEngine;

} // namespace aknet::startup

#endif //AKNET_STARTUP_ENGINE_H
