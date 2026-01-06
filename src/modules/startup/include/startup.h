//
// Created by Nicolas Désilles on 06/01/2026.
//

#ifndef AKNET_STARTUP_H
#define AKNET_STARTUP_H

#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <optional>

namespace aknet::startup {

    // -------------------------------------------------------------------------
    // Data structs definitions
    // -------------------------------------------------------------------------

    enum class AppState { Off, Booting, Active, ShuttingDown };

    enum class StepStatus { Pending, Running, Success, Failed, TimedOut, Skipped, Aborted };

    struct StepConfig {
        std::string id;
        std::string display_name;
        std::chrono::seconds timeout{0};
        bool critical = false;
        bool can_skip = false;
    };

    struct StepProgress {
        std::string id;
        std::string display_name;
        StepStatus status = StepStatus::Pending;
        std::string message;
        std::optional<std::chrono::time_point<std::chrono::steady_clock>> start_time;
        std::optional<std::chrono::time_point<std::chrono::steady_clock>> end_time;
    };

    struct SequenceProgress {
        AppState state = AppState::Off;
        int current_step_index = -1;
        std::vector<StepProgress> steps;
        std::optional<std::string> last_error;
        bool can_retry = false;
    };

    struct Result {
        bool ok = true;
        std::string error;
    };

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    StepProgress make_initial_step_progress(const StepConfig& config);

    SequenceProgress make_initial_sequence_progress(AppState state, const std::vector<StepConfig>& configs);

    Result validate_step_config(const StepConfig& config);

    Result validate_step_configs_unique(const std::vector<StepConfig>& configs);

}

#endif //AKNET_STARTUP_H