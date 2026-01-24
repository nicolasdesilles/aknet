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

    /**
     * State of the application
     */
    enum class AppState {
        Off,          ///< "Idle" state, default when starting the app
        Booting,      ///< When the startup process is running
        Active,       ///< When the app is fully active (audio engine active, networking, etc)
        ShuttingDown  ///< When the app is shutting down
    };

    /**
     * Status of a step in the startup process
     */
    enum class StepStatus {
        Pending,   ///< The step is yet to be started
        Running,   ///< The step is in progress
        Success,   ///< The step is finished, and was a success
        Failed,    ///< The step is finished, and failed
        TimedOut,  ///< The step has timed out (execution time exceeded the step timeout value)
        Skipped,   ///< The step was skipped
        Aborted    ///< The step was aborted (when the whole startup process is stopped)
    };

    enum class AbortReason {
        None,              ///< No abort requested
        UserRequested,     ///< User clicked "Cancel" button
        Timeout,           ///< Step exceeded its timeout (future use)
        CriticalFailure,   ///< Critical step failed (future use)
        SystemShutdown     ///< Application is shutting down
    };

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
        AbortReason abort_reason = AbortReason::None;
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