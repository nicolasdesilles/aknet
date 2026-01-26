//
// startup_manager.h - Thread-safe asynchronous startup sequence manager.
//

#ifndef AKNET_STARTUP_MANAGER_H
#define AKNET_STARTUP_MANAGER_H

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <functional.hpp>
#include <ereignis/manager/manager.hpp>

#include "startup.h"
#include "startup_engine.h"

namespace aknet::log {
    class Logger;
}

namespace aknet::settings {
    class Settings;
}

namespace aknet::jack {
    class JackModule;
}

namespace aknet::startup {

    /**
     * Thread-safe asynchronous startup manager with event system.
     */
    namespace manager {

        /**
         * Thread-safe asynchronous startup sequence manager.
     *
     * Provides async execution of startup steps with thread-safe state access and an event system for progress notifications.
     * This is the primary interface for application code to interact with the startup system.
     *
     * @info
     * StartupManager is designed to be owned by the core module.
     * It spawns a background worker thread that executes the startup sequence.
     * Events are fired from the worker thread - subscribers must handle thread safety.
     *
     * #### Thread Safety
     *
     * - All public methods are thread-safe
     * - Progress queries return copies, not references
     * - Step modification is blocked while sequence is running
     * - Events fire from the worker thread
     *
     * #### Event System
     *
     * Subscribe to events using the events() accessor:
     *
     * ```cpp
     * manager->events().get<StartupManager::Event::ProgressChanged>()
     *     .on([](const SequenceProgress& p) {
     *         // Handle progress update (called from worker thread!)
     *     });
     * ```
     *
     * #### Lifecycle
     *
     * 1. Construct with logger and settings
     * 2. Register steps with set_steps() or add_step()
     * 3. Subscribe to events
     * 4. Call start_async() to begin execution
     * 5. Handle events (ProgressChanged, SequenceCompleted, etc.)
     * 6. On failure with can_retry=true, call retry_async()
     * 7. Destructor waits for completion and cleans up
     */
    class StartupManager final {
    public:
        /**
         * Unique pointer to a startup step.
         */
        using StepPtr = std::unique_ptr<IStartupStep>;

        /**
         * Construct a StartupManager.
         *
         * Spawns a background worker thread that waits for start signals.
         *
         * @param logger Logger for diagnostic output.
         * @param settings Settings access for steps.
         * @param clock Clock for timeout checking (defaults to SteadyClock).
         */
        StartupManager(
            std::shared_ptr<log::Logger> logger,
            const std::shared_ptr<settings::Settings> &settings,
            std::shared_ptr<IClock> = std::make_shared<SteadyClock>());

        /**
         * Destructor.
         *
         * Signals the worker thread to stop, aborts any running sequence (with SystemShutdown reason), and waits for thread completion.
         */
        ~StartupManager();

        // Non-copyable, non-movable
        StartupManager(const StartupManager&) = delete;
        StartupManager& operator=(const StartupManager&) = delete;
        StartupManager(StartupManager&&) noexcept = delete;
        StartupManager& operator=(StartupManager&&) noexcept = delete;

        // -- Step Management --

        /**
         * Replace all registered steps.
         *
         * Cannot be called while sequence is running.
         *
         * @param steps Vector of steps to register (ownership transferred).
         *
         * @return Result indicating success or error (e.g., if running).
         */
        Result set_steps(std::vector<StepPtr> steps);

        /**
         * Append a step to the existing sequence.
         *
         * Cannot be called while sequence is running.
         *
         * @param step Step to add (ownership transferred).
         *
         * @return Result indicating success or error (e.g., if running).
         */
        Result add_step(StepPtr step);

        /**
         * Remove all registered steps.
         *
         * No-op if sequence is running.
         */
        void clear_steps();

        // -- Async Execution --

        /**
         * Start the startup sequence asynchronously.
         *
         * Returns immediately; execution happens on the worker thread.
         * Subscribe to events to receive progress updates.
         *
         * @return Result indicating whether start was initiated.
         *
         * #### Errors
         *
         * - Returns error if already running
         * - Returns error if no steps configured
         */
        Result start_async();

        /**
         * Request abort of the running sequence.
         *
         * Thread-safe; can be called from any thread.
         * The abort is processed between steps or when the current step checks for abort.
         *
         * @param reason Why the abort is being requested (default: UserRequested).
         */
        void request_abort(AbortReason reason = AbortReason::UserRequested);

        /**
         * Retry the startup sequence asynchronously.
         *
         * Only succeeds if not running and can_retry is true.
         *
         * @return Result indicating whether retry was initiated.
         */
        Result retry_async();

        // -- Thread-Safe Accessors --

        /**
         * Get a copy of the current progress.
         *
         * @return Copy of SequenceProgress (safe to use from any thread).
         */
        SequenceProgress get_progress() const;

        /**
         * Get the current application state.
         *
         * @return Current AppState.
         */
        AppState get_state() const;

        /**
         * Check if the sequence is currently running.
         *
         * @return True if sequence is executing.
         */
        bool is_running() const;

        /**
         * Check if retry is allowed.
         *
         * @return True if last run failed with can_retry=true and not currently running.
         */
        bool can_retry() const;

        /**
        * Set the JackModule for steps to use.
        *
        * @param jack_module Shared pointer to JackModule.
        */
        void set_jack_module(std::shared_ptr<jack::JackModule> jack_module);

        // -- Event System --

        /**
         * Event types fired by StartupManager.
         *
         */
        enum class Event : std::uint8_t {
            ProgressChanged,     ///< Fired after each step (with full progress snapshot)
            StateChanged,        ///< Fired on AppState transitions (old_state, new_state)
            StepStarted,         ///< Fired when step begins (index, id)
            StepCompleted,       ///< Fired when step finishes (index, id, status)
            SequenceCompleted,   ///< Fired when sequence finishes (success, error_msg)
            Error                ///< Fired on critical errors (error_msg)
        };

        /**
         * Event manager type.
         *
         * All events are fired from the worker thread.
         * Subscribers must handle thread safety in their handlers.
         */
        using Events = ereignis::manager<
            ereignis::event<Event::ProgressChanged, void(const SequenceProgress&)>,
            ereignis::event<Event::StateChanged, void(AppState, AppState)>,
            ereignis::event<Event::StepStarted, void(int, const std::string&)>,
            ereignis::event<Event::StepCompleted, void(int, const std::string&, StepStatus)>,
            ereignis::event<Event::SequenceCompleted, void(bool, const std::string&)>,
            ereignis::event<Event::Error, void(const std::string&)>
        >;

        /**
         * Access the event manager for subscribing to events.
         *
         * @return Reference to the Events manager.
         *
         * #### Example
         *
         * ```cpp
         * // Subscribe to progress changes
         * manager->events().get<Event::ProgressChanged>()
         *     .on([](const SequenceProgress& progress) {
         *         auto json = serialize_progress(progress);
         *         send_to_ui(json);
         *     });
         *
         * // Subscribe to completion
         * manager->events().get<Event::SequenceCompleted>()
         *     .on([](bool success, const std::string& error) {
         *         if (success) {
         *             transition_to_main_ui();
         *         } else {
         *             show_error_dialog(error);
         *         }
         *     });
         * ```
         */
        Events& events() { return events_; }


    private:
        /**
         * Worker thread function.
         */
        void worker_thread_fn();

        /**
         * Logger for diagnostic output.
         */
        std::shared_ptr<log::Logger> logger_;

        /**
         * Underlying synchronous engine.
         */
        std::shared_ptr<StartupEngine> engine_;

        /**
         * Background worker thread.
         */
        std::thread worker_thread_;

        /**
         * True when worker thread is ready to receive signals.
         */
        std::atomic<bool> thread_ready_{false};

        /**
         * Signal to worker thread to start execution.
         */
        std::atomic<bool> should_run_{false};

        /**
         * Signal to worker thread to stop.
         */
        std::atomic<bool> should_stop_{false};

        /**
         * True while sequence is executing.
         */
        std::atomic<bool> is_running_{false};

        /**
         * Event manager.
         */
        Events events_;

        /**
         * Mutex protecting engine and cached_progress_.
         */
        mutable std::mutex mutex_;

        /**
         * Condition variable for thread signaling.
         */
        std::condition_variable cv_;

        /**
         * Cached progress snapshot for thread-safe reads.
         */
        SequenceProgress cached_progress_;

    };

    } // namespace manager

    // Re-export at startup:: level for API convenience
    using manager::StartupManager;

} // namespace aknet::startup

#endif //AKNET_STARTUP_MANAGER_H
