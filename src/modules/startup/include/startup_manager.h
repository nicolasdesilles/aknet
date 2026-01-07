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


    private:
        void worker_thread_fn();

        std::shared_ptr<log::Logger> logger_;
        std::shared_ptr<StartupEngine> engine_;

        std::thread worker_thread_;
        std::atomic<bool> should_run_{false};
        std::atomic<bool> should_stop_{false};
        std::atomic<bool> is_running_{false};

        // mutex
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        SequenceProgress cached_progress_;

    };

}

#endif //AKNET_STARTUP_MANAGER_H