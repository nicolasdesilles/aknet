//
// Created by Nicolas Désilles on 07/01/2026.
//

#include <logger.h>
#include <settings.h>

#include "startup_manager.h"

namespace aknet::startup {

    StartupManager::StartupManager(
        std::shared_ptr<log::Logger> logger,
        const std::shared_ptr<settings::Settings>& settings,
        std::shared_ptr<IClock> clock)
    {
        logger_ = std::move(logger);
        engine_ = std::make_unique<StartupEngine>(logger_, settings, std::move(clock));

        worker_thread_ = std::thread(&StartupManager::worker_thread_fn, this);

        // Wait for thread to reach its wait state
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] {
                return thread_ready_.load(std::memory_order_acquire);
            });
        }

        logger_->info("Startup manager initialized.");
    }

    StartupManager::~StartupManager() {
        // Signal thread to stop
        should_stop_.store(true, std::memory_order_release);
        cv_.notify_one();

        // If sequence is running, abort it
        if (is_running_.load(std::memory_order_acquire)) {
            engine_->request_abort(AbortReason::SystemShutdown);
        }

        // Wait for thread to finish
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    // Step management

    Result StartupManager::set_steps(std::vector<StepPtr> steps) {
        std::lock_guard lock(mutex_);

        if (is_running_.load(std::memory_order_acquire)) {
            logger_->warn("StartupManager::set_steps called while sequence is running");
            return {false, "Cannot set steps while sequence is running"};
        }

        auto result = engine_->set_steps(std::move(steps));
        if (result.ok) {
            cached_progress_ = engine_->progress();
        }
        return result;
    }

    Result StartupManager::add_step(StepPtr step) {
        std::lock_guard lock(mutex_);

        if (is_running_.load(std::memory_order_acquire)) {
            logger_->warn("StartupManager::add_step called while sequence is running");
            return {false, "Cannot add step while sequence is running"};
        }

        auto result = engine_->add_step(std::move(step));
        if (result.ok) {
            cached_progress_ = engine_->progress();
        }
        return result;
    }

    void StartupManager::clear_steps() {
        std::lock_guard lock(mutex_);

        if (!is_running_.load(std::memory_order_acquire)) {
            engine_->clear_steps();
            cached_progress_ = engine_->progress();
        }
    }

    // Async execution

    Result StartupManager::start_async() {
        std::lock_guard lock(mutex_);

        if (is_running_.load(std::memory_order_acquire)) {
            logger_->warn("StartupManager::start_async called while sequence is running");
            return {false, "Sequence is already running"};
        }

        if (!engine_->has_steps()) {
            return {false, "No steps configured"};
        }

        // Set running flag immediately to prevent concurrent starts
        is_running_.store(true, std::memory_order_release);

        // Signal worker thread to start
        should_run_.store(true, std::memory_order_release);
        cv_.notify_one();

        return {true};
    }

    void StartupManager::request_abort(AbortReason reason) {
        // this should be thread safe
        engine_->request_abort(reason);
    }

    Result StartupManager::retry_async() {
        std::lock_guard lock(mutex_);

        if (is_running_.load(std::memory_order_acquire)) {
            return {false, "Sequence is already running"};
        }

        if (!engine_->can_retry()) {
            return {false, "Cannot retry: last run did not fail with can_retry=true"};
        }

        // Set running flag immediately to prevent concurrent starts
        is_running_.store(true, std::memory_order_release);

        // Signal worker thread to retry
        should_run_.store(true, std::memory_order_release);
        cv_.notify_one();

        return {true};
    }

    // Thread-safe accessors

    SequenceProgress StartupManager::get_progress() const {
        std::lock_guard lock(mutex_);
        return cached_progress_;
    }

    AppState StartupManager::get_state() const {
        std::lock_guard lock(mutex_);
        return cached_progress_.state;
    }

    bool StartupManager::is_running() const {
        return is_running_.load(std::memory_order_acquire);
    }

    bool StartupManager::can_retry() const {
        std::lock_guard lock(mutex_);
        return engine_->can_retry();
    }

    // Worker thread

    void StartupManager::worker_thread_fn() {
        logger_->debug("StartupManager worker thread started");

        // Signal that we're ready
        {
            std::lock_guard lock(mutex_);
            thread_ready_.store(true, std::memory_order_release);
        }
        cv_.notify_one();  // Wake up constructor

        while (!should_stop_.load(std::memory_order_acquire)) {
            std::unique_lock lock(mutex_);

            // Wait for signal to run or stop
            cv_.wait(lock, [this] {
                return should_run_.load(std::memory_order_acquire) ||
                       should_stop_.load(std::memory_order_acquire);
            });

            // Check if we should stop
            if (should_stop_.load(std::memory_order_acquire)) {
                break;
            }

            // Clear run signal (should_run_ is guaranteed to be true here)
            should_run_.store(false, std::memory_order_release);

            // Check if we should retry or run fresh (while holding lock)
            bool should_retry = engine_->can_retry();

            // Capture old state for event (use cached_progress_)
            AppState old_state = cached_progress_.state;

            // Release lock before running sequence
            lock.unlock();

            logger_->info("StartupManager executing sequence...");

            // Fire StateChanged: old_state to Booting
            AppState starting_state = AppState::Booting;
            if (old_state != starting_state) {
                events_.get<Event::StateChanged>().fire(old_state, starting_state);
            }

            // Run sequence without holding lock

            StartupEngine::RunOptions run_options;

            run_options.progress_callback = [this](const SequenceProgress& progress) {
                events_.get<Event::ProgressChanged>().fire(progress);
            };

            run_options.step_started_callback = [this](int index, const std::string& id) {
                events_.get<Event::StepStarted>().fire(index, id);
            };

            run_options.step_completed_callback = [this](int index, const std::string& id, StepStatus status) {
                events_.get<Event::StepCompleted>().fire(index, id, status);
            };

            if (should_retry) {
                engine_->retry(run_options);
            } else {
                engine_->run(run_options);
            }

            // Re-acquire lock to update cached progress
            lock.lock();

            cached_progress_ = engine_->progress();
            AppState final_state = cached_progress_.state;

            // Clear running flag after completion
            is_running_.store(false, std::memory_order_release);

            logger_->info("StartupManager sequence completed: state={}",
                         static_cast<int>(cached_progress_.state));

            // Release lock before firing events
            lock.unlock();

            // Fire StateChanged: Booting → Active/Off
            if (starting_state != final_state) {
                events_.get<Event::StateChanged>().fire(starting_state, final_state);
            }

            // Fire Error event if there's a critical error
            if (cached_progress_.last_error.has_value() && !cached_progress_.last_error.value().empty()) {
                events_.get<Event::Error>().fire(cached_progress_.last_error.value());
            }

            // Fire SequenceCompleted event
            bool success = (final_state == AppState::Active);
            std::string error_msg = cached_progress_.last_error.value_or("");
            events_.get<Event::SequenceCompleted>().fire(success, error_msg);

        }

        logger_->debug("StartupManager worker thread stopped");
    }

}
