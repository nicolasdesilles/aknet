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

        rebuild_progress_snapshot_(AppState::Off);

        progress_.last_error = "";
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
        rebuild_progress_snapshot_(AppState::Off);

        return {.ok = true};
    }

    void StartupEngine::clear_steps() {

        steps_.clear();

        progress_ = {
            .state = AppState::Off,
            .current_step_index = -1,
            .steps = {},
            .last_error = "",
            .can_retry = false
        };

    }

    // Accessors

    const SequenceProgress & StartupEngine::progress() const {
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
    };





}


