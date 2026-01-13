//
// Created by Nicolas Désilles on 07/01/2026.
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

namespace aknet::startup {

    // thread-safe async wrapper around StartupEngine
    class StartupManager final {
    public:
        using StepPtr = std::unique_ptr<IStartupStep>;

        // Constructor
        StartupManager(
            std::shared_ptr<log::Logger> logger,
            const std::shared_ptr<settings::Settings> &settings,
            std::shared_ptr<IClock> = std::make_shared<SteadyClock>());

        // Destructor (not default)
        ~StartupManager();

        // Non copyable, non movable
        StartupManager(const StartupManager&) = delete;
        StartupManager& operator=(const StartupManager&) = delete;
        StartupManager(StartupManager&&) noexcept = delete;
        StartupManager& operator=(StartupManager&&) noexcept = delete;

        // Step management, should be called when not running
        Result set_steps(std::vector<StepPtr> steps);
        Result add_step(StepPtr step);
        void clear_steps();

        // Async exec
        Result start_async();
        void request_abort(AbortReason reason = AbortReason::UserRequested);
        Result retry_async();

        // thread safe accessors
        SequenceProgress get_progress() const;
        AppState get_state() const;
        bool is_running() const;
        bool can_retry() const;

        // Event System

        // Event types that StartupManager can fire
        enum class Event : std::uint8_t {
            ProgressChanged,     // Fired after each step completes (with full progress snapshot)
            StateChanged,        // Fired on AppState transitions (e.g., Off→Booting→Active)
            StepStarted,         // Fired when a step begins execution
            StepCompleted,       // Fired when a step finishes (success/fail/timeout/abort)
            SequenceCompleted,   // Fired when entire sequence finishes
            Error                // Fired on critical errors
        };

        // Event manager type (ereignis)
        // All events are fired from the worker thread.
        // Subscribers will be called from the worker thread - ensure thread safety in handlers.
        using Events = ereignis::manager<
            ereignis::event<Event::ProgressChanged, void(const SequenceProgress&)>,
            ereignis::event<Event::StateChanged, void(AppState, AppState)>,  // old_state, new_state
            ereignis::event<Event::StepStarted, void(int, const std::string&)>,  // index, id
            ereignis::event<Event::StepCompleted, void(int, const std::string&, StepStatus)>,  // index, id, status
            ereignis::event<Event::SequenceCompleted, void(bool, const std::string&)>,  // success, error_msg
            ereignis::event<Event::Error, void(const std::string&)>  // error_msg
        >;

        // Event Access

        // Get the event manager to subscribe to events
        Events& events() { return events_; }
        const Events& events() const { return events_; }


    private:
        void worker_thread_fn();

        std::shared_ptr<log::Logger> logger_;
        std::shared_ptr<StartupEngine> engine_;

        std::thread worker_thread_;
        std::atomic<bool> should_run_{false};
        std::atomic<bool> should_stop_{false};
        std::atomic<bool> is_running_{false};

        Events events_;

        // mutex
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        SequenceProgress cached_progress_;

    };

}

#endif //AKNET_STARTUP_MANAGER_H