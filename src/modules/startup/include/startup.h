//
// startup.h - Core data types for the startup module.
//
// This header defines the fundamental enums, structs, and helper functions
// used throughout the startup system.
//

#ifndef AKNET_STARTUP_H
#define AKNET_STARTUP_H

#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <optional>

namespace aknet::startup {
    /**
     * Core data types, enums, and structs for the startup module.
     */
    namespace types {

        // =========================================================================
        // Enums
        // =========================================================================

        /**
         * Application lifecycle state.
         *
         * Represents the high-level state of the application as it transitions through the startup process.
         */
        enum class AppState {
            Off,          ///< Idle state, default when starting the app or after failure
            Booting,      ///< Startup process is running
            Active,       ///< App is fully active (audio engine, networking, etc.)
            ShuttingDown  ///< App is shutting down
        };

        /**
         * Execution status of a single startup step.
         *
         * Tracks the lifecycle of an individual step from pending through completion.
         */
        enum class StepStatus {
            Pending,   ///< Step is yet to be started
            Running,   ///< Step is in progress
            Success,   ///< Step finished successfully
            Failed,    ///< Step finished with failure
            TimedOut,  ///< Step exceeded its timeout value
            Skipped,   ///< Step was skipped
            Aborted    ///< Step was aborted (startup process stopped)
        };

        /**
         * Reason for aborting the startup sequence.
         *
         * When the startup sequence is aborted, this enum indicates why.
         * Used for logging, UI display, and determining whether retry is appropriate.
         */
        enum class AbortReason {
            None,              ///< No abort requested (normal operation)
            UserRequested,     ///< User clicked "Cancel" or similar
            Timeout,           ///< Step exceeded its timeout (future use)
            CriticalFailure,   ///< Critical step failed (future use)
            SystemShutdown     ///< Application is shutting down
        };

        // =========================================================================
        // Configuration and Progress Structs
        // =========================================================================

        /**
         * Static configuration for a startup step.
         *
         * Defines the immutable properties of a step that are known at registration time.
         * These values do not change during execution.
         */
        struct StepConfig {
            std::string id;                              ///< Unique step identifier
            std::string display_name;                    ///< Human-readable name for UI
            std::chrono::seconds timeout{0};             ///< Max execution time (0 = unlimited)
            bool critical = false;                       ///< If true, failure stops sequence
            bool can_skip = false;                       ///< If true, step can be skipped
        };

        /**
         * Runtime progress information for a single step.
         *
         * Tracks the current state and timing of a step during and after execution.
         * Updated by the engine as the step runs.
         */
        struct StepProgress {
            std::string id;                              ///< Step identifier (matches StepConfig::id)
            std::string display_name;                    ///< Human-readable name (copied from config)
            StepStatus status = StepStatus::Pending;     ///< Current execution status
            std::string message;                         ///< Status message or error description
            std::optional<std::chrono::time_point<std::chrono::steady_clock>> start_time;  ///< When step began
            std::optional<std::chrono::time_point<std::chrono::steady_clock>> end_time;    ///< When step finished
        };

        /**
         * Complete snapshot of startup sequence progress.
         *
         * Contains the full state of the startup process at a point in time.
         * Used for UI updates and state queries.
         * Designed to be copied and passed around safely (immutable snapshot pattern).
         */
        struct SequenceProgress {
            AppState state = AppState::Off;              ///< Current application state
            int current_step_index = -1;                 ///< Index of running step (-1 if none)
            std::vector<StepProgress> steps;             ///< Progress for all registered steps
            std::optional<std::string> last_error;       ///< Error message if sequence failed
            bool can_retry = false;                      ///< True if retry is allowed after failure
            AbortReason abort_reason = AbortReason::None;  ///< Why sequence was aborted (if applicable)
        };

        /**
         * Simple result type for operations that can fail.
         *
         * Used throughout the startup module for error handling without exceptions.
         */
        struct Result {
            bool ok = true;       ///< True if operation succeeded
            std::string error;    ///< Error message if ok is false
        };

        // =========================================================================
        // Helper Functions
        // =========================================================================

        /**
         * Create initial StepProgress from a StepConfig.
         *
         * @param config The step configuration.
         *
         * @return StepProgress with status=Pending and id/display_name copied from config.
         */
        StepProgress make_initial_step_progress(const StepConfig& config);

        /**
         * Create initial SequenceProgress for a set of steps.
         *
         * @param state Initial AppState to set.
         * @param configs Vector of step configurations.
         *
         * @return SequenceProgress with all steps in Pending status.
         */
        SequenceProgress make_initial_sequence_progress(AppState state, const std::vector<StepConfig>& configs);

        /**
         * Validate a single step configuration.
         *
         * Checks that id and display_name are non-empty and timeout is non-negative.
         *
         * @param config The step configuration to validate.
         *
         * @return Result with ok=true if valid, or error message if invalid.
         */
        Result validate_step_config(const StepConfig& config);

        /**
         * Validate that all step IDs in a collection are unique.
         *
         * @param configs Vector of step configurations to check.
         *
         * @return Result with ok=true if all unique, or error message naming the duplicate.
         */
        Result validate_step_configs_unique(const std::vector<StepConfig>& configs);

    } // namespace types

    // =========================================================================
    // Re-export types at startup:: level for API convenience
    // =========================================================================

    using types::AppState;
    using types::StepStatus;
    using types::AbortReason;
    using types::StepConfig;
    using types::StepProgress;
    using types::SequenceProgress;
    using types::Result;
    using types::make_initial_step_progress;
    using types::make_initial_sequence_progress;
    using types::validate_step_config;
    using types::validate_step_configs_unique;

}

#endif //AKNET_STARTUP_H
