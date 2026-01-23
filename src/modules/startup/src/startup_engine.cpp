//
// Created by Nicolas Désilles on 06/01/2026.
//

#include "startup_engine.h"


namespace aknet::startup {

    // -------------------------------------------------------------------------
    // StartupEngine Class implementation
    // -------------------------------------------------------------------------

    // Constructor
    StartupEngine::StartupEngine(
        std::shared_ptr<log::Logger> logger,
        std::shared_ptr<settings::Settings> settings,
        std::shared_ptr<IClock> clock) {

        if (!logger) {
            throw std::invalid_argument("A valid Logger must be provided to StartupEngine on creation");
        }

        if (!settings) {
            throw std::invalid_argument("A valid Settings must be provided to StartupEngine on creation");
        }

        if (!clock) {
            throw std::invalid_argument("A valid Clock must be provided to StartupEngine on creation");
        }

        // Storing members
        logger_ = std::move(logger);
        settings_ = std::move(settings);
        clock_ = std::move(clock);

        // Init progress
        progress_.state = AppState::Off;
        progress_.current_step_index = -1;

    }

    // Step Management

    Result StartupEngine::set_steps(std::vector<StepPtr> steps) {

        // first we validate the steps

        if (steps.empty()) {
            logger_->error("No steps provided to StartupEngine::set_steps");
            return {.ok = false, .error = "No steps provided"};
        }

        std::vector<StepConfig> configs;

        for (auto & step : steps) {
            if (!step) {
                return {.ok = false, .error = "Invalid step provided"};
            }
            configs.push_back(step->config());
        }

        for (auto & config : configs) {
            auto val_res = validate_step_config(config);
            if (!val_res.ok) {
                logger_->error("Invalid step config provided: {}", val_res.error);
                return {.ok = false, .error = val_res.error};
            }
        }

        auto val_res = validate_step_configs_unique(configs);
        if (!val_res.ok) {
            logger_->error("Invalid step configs provided: {}", val_res.error);
            return {.ok = false, .error = val_res.error};
        }

        steps_ = std::move(steps);

        rebuild_progress_snapshot(AppState::Off);

        progress_.last_error.reset();
        progress_.can_retry = false;
        progress_.current_step_index = -1;

        return {.ok = true};

    }

    Result StartupEngine::add_step(StepPtr step) {

        if (!step) {
            logger_->error("Invalid step provided to StartupEngine::add_step");
            return {.ok = false, .error = "Invalid step provided"};
        }

        auto config = step->config();
        auto val_res = validate_step_config(config);
        if (!val_res.ok) {
            logger_->error("Invalid step config provided: {}", val_res.error);
            return {.ok = false, .error = val_res.error};
        }

        std::vector<StepConfig> current_configs;
        current_configs.reserve(steps_.size() + 1);
        for (auto & s : steps_) {
            current_configs.push_back(s->config());
        }

        current_configs.push_back(config);

        auto val_res2 = validate_step_configs_unique(current_configs);
        if (!val_res2.ok) {
            logger_->error("Invalid step configs provided: {}", val_res2.error);
            return {.ok = false, .error = val_res2.error};
        }

        steps_.push_back(std::move(step));
        rebuild_progress_snapshot(AppState::Off);

        return {.ok = true};
    }

    void StartupEngine::clear_steps() {

        steps_.clear();

        progress_ = {
            .state = AppState::Off,
            .current_step_index = -1,
            .steps = {},
            .last_error = std::nullopt,
            .can_retry = false
        };

    }

    // Run sequence

    const SequenceProgress & StartupEngine::run(const RunOptions &options) {

        logger_->info("Running startup sequence...");

        // Store callbacks for use in run_step()
        current_progress_callback_ = options.progress_callback;
        current_step_started_callback_ = options.step_started_callback;
        current_step_completed_callback_ = options.step_completed_callback;

        if (steps_.empty()) {
            logger_->error("No steps provided to StartupEngine::run");

            progress_.last_error = "No steps provided";
            progress_.state = AppState::Off;
            progress_.can_retry = false;

            return progress_;
        }

        if (options.reset_progress_before_run) {
            rebuild_progress_snapshot(AppState::Booting);
        }
        else {
            progress_.state = AppState::Booting;
        }

        progress_.last_error.reset();
        progress_.can_retry = false;
        
        // Sync abort reason from atomic to progress snapshot
        progress_.abort_reason = abort_reason_.load(std::memory_order_relaxed);

        if (abort_requested_) {
            // Ensure abort reason is set if not already
            if (progress_.abort_reason == AbortReason::None) {
                progress_.abort_reason = AbortReason::UserRequested;
            }
            transition_to_off_with_error("Startup aborted", false);
            return progress_;
        }

        for (std::size_t i = 0; i < steps_.size(); ++i) {

            if (abort_requested_) {
                // Sync abort reason from atomic to progress
                progress_.abort_reason = abort_reason_.load(std::memory_order_relaxed);
                // If abort reason is None, set it to UserRequested as default
                if (progress_.abort_reason == AbortReason::None) {
                    progress_.abort_reason = AbortReason::UserRequested;
                }

                // Mark all steps as aborted
                for (std::size_t j = i; j < progress_.steps.size(); ++j) {
                    progress_.steps[j].status = StepStatus::Aborted;
                }

                transition_to_off_with_error("Startup aborted", false);
                return progress_;
            }

            bool continue_sequence = run_step(i);

            if (options.progress_callback) {
                options.progress_callback(progress_);
            }

            if (!continue_sequence) {
                return progress_;
            }

        }

        logger_->info("Startup sequence completed successfully.");

        progress_.state = AppState::Active;
        progress_.current_step_index = -1;

        // Fire final progress callback
        if (options.progress_callback) {
            options.progress_callback(progress_);
        }

        // Clear callbacks after run
        current_progress_callback_ = nullptr;
        current_step_started_callback_ = nullptr;
        current_step_completed_callback_ = nullptr;

        return progress_;

    }

    Result StartupEngine::retry(const RunOptions& options) {

        // we can only retry if the last run failed and can_retry is true
        if (progress_.state != AppState::Off) {
            logger_->warn("Cannot retry sequence run, sequence is not in Off state");
            return {.ok = false, .error = "Cannot retry sequence run, sequence is not in Off state"};
        }

        if (!progress_.can_retry) {
            logger_->warn("Cannot retry sequence run, sequence cannot be retried");
            return {.ok = false, .error = "Cannot retry sequence run, sequence cannot be retried"};
        }

        logger_->info("Retrying startup sequence...");

        reset_abort();

        rebuild_progress_snapshot(AppState::Booting);

        // run the sequence again
        run(options);

        return {.ok = true};
    }

    // Cancellation

    void StartupEngine::request_abort(AbortReason reason) {
        abort_reason_.store(reason, std::memory_order_relaxed);
        abort_requested_.store(true, std::memory_order_relaxed);
    }

    void StartupEngine::reset_abort() {
        abort_requested_.store(false, std::memory_order_relaxed);
        abort_reason_.store(AbortReason::None, std::memory_order_relaxed);
    }

    // Accessors

    const SequenceProgress & StartupEngine::progress() const {
        // Sync atomic abort_reason to progress snapshot before returning
        const_cast<StartupEngine*>(this)->progress_.abort_reason = 
            abort_reason_.load(std::memory_order_relaxed);
        return progress_;
    }

    AppState StartupEngine::state() const {
        return progress_.state;
    }

    bool StartupEngine::has_steps() const {
        return !steps_.empty();
    }

    std::size_t StartupEngine::step_count() const {
        return steps_.size();
    }

    bool StartupEngine::can_retry() const {
        return progress_.state == AppState::Off && progress_.can_retry;
    }

    // Rebuild progress snapshot

    void StartupEngine::rebuild_progress_snapshot(AppState state) {

        std::vector<StepConfig> configs;
        configs.reserve(steps_.size());

        for (auto & step : steps_) {
            configs.push_back(step->config());
        }

        progress_ = make_initial_sequence_progress(state, configs);
        progress_.current_step_index = -1;
        progress_.last_error.reset();
        progress_.can_retry = false;

    }

    // Run step

    bool StartupEngine::run_step(std::size_t index) {

        progress_.current_step_index = static_cast<int>(index);

        auto &step_progress = progress_.steps[index];
        step_progress.status = StepStatus::Running;
        step_progress.start_time = clock_->now();
        step_progress.message = "";

        auto &step_config = steps_[index]->config();
        std::chrono::time_point<std::chrono::steady_clock> deadline;

        if (step_config.timeout == std::chrono::seconds::zero()) {
            deadline = std::chrono::time_point<std::chrono::steady_clock>::max();
        }
        else {
            deadline = clock_->now() + step_config.timeout;
        }

        StepContext ctx{};
        ctx.abort_flag = &abort_requested_;
        ctx.clock = clock_.get();
        ctx.deadline = deadline;
        ctx.logger = logger_;
        ctx.settings = settings_.get();

        logger_->info("Running step {}...", steps_[index]->config().id);

        // Fire StepStarted callback
        if (current_step_started_callback_) {
            current_step_started_callback_(static_cast<int>(index), step_config.id);
        }

        StepResult result = steps_[index]->run(ctx);

        //Check timeout
        if (ctx.is_expired() && step_config.timeout != std::chrono::seconds::zero()) {
            result.status = StepStatus::TimedOut;
            result.message = "Step exceeded timeout of " + std::to_string(step_config.timeout.count()) + "s";
        }

        if (ctx.abort_requested()) {
            step_progress.status = StepStatus::Aborted;
            // Sync abort reason from atomic to progress
            progress_.abort_reason = abort_reason_.load(std::memory_order_relaxed);
            // If abort reason is None, set it to UserRequested as default
            if (progress_.abort_reason == AbortReason::None) {
                progress_.abort_reason = AbortReason::UserRequested;
            }

            // Mark all remaining steps as aborted
            for (std::size_t j = index + 1; j < progress_.steps.size(); ++j) {
                progress_.steps[j].status = StepStatus::Aborted;
            }

            transition_to_off_with_error("Step " + step_config.id + " aborted", false);
            return false; // Stop sequence run
        }

        step_progress.status = result.status;
        step_progress.end_time = clock_->now();
        step_progress.message = result.message;

        if (result.status == StepStatus::Failed && step_config.critical) {

            transition_to_off_with_error("Step " + step_config.id + " failed and was critical", true);
            return false; // Stop sequence run

        }

        if (result.status == StepStatus::TimedOut && step_config.critical) {

            transition_to_off_with_error("Step " + step_config.id + " timed out and was critical", true);
            return false; // Stop sequence run

        }

        if (result.status == StepStatus::Aborted && step_config.critical) {

            transition_to_off_with_error("Step " + step_config.id + " was aborted and was critical", true);
            return false; // Stop sequence run

        }

        if (current_step_completed_callback_) {
            current_step_completed_callback_(static_cast<int>(index), step_config.id, step_progress.status);
        }

        return true;

    }

    void StartupEngine::transition_to_off_with_error(std::string error, bool can_retry) {

        progress_.state = AppState::Off;
        progress_.last_error = std::move(error);
        progress_.can_retry = can_retry;
        progress_.current_step_index = -1;

    };





}


