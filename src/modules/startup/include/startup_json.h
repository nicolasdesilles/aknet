//
// startup_json.h - JSON serialization for startup module types.
//

#ifndef AKNET_STARTUP_JSON_H
#define AKNET_STARTUP_JSON_H

#pragma once

#include "startup.h"
#include <nlohmann/json.hpp>

namespace aknet::startup::types {

    // =========================================================================
    // Enum Serialization
    // =========================================================================

    /**
     * Serialize AppState to JSON (as integer).
     */
    void to_json(nlohmann::json& j, AppState state);

    /**
     * Deserialize AppState from JSON.
     */
    void from_json(const nlohmann::json& j, AppState& state);

    /**
     * Serialize StepStatus to JSON (as integer).
     */
    void to_json(nlohmann::json& j, StepStatus status);

    /**
     * Deserialize StepStatus from JSON.
     */
    void from_json(const nlohmann::json& j, StepStatus& status);

    /**
     * Serialize AbortReason to JSON (as integer).
     */
    void to_json(nlohmann::json& j, AbortReason reason);

    /**
     * Deserialize AbortReason from JSON.
     */
    void from_json(const nlohmann::json& j, AbortReason& reason);

    // =========================================================================
    // Struct Serialization (ADL-compatible)
    // =========================================================================

    /**
     * Serialize StepConfig to JSON.
     */
    void to_json(nlohmann::json& j, const StepConfig& config);

    /**
     * Deserialize StepConfig from JSON.
     */
    void from_json(const nlohmann::json& j, StepConfig& config);

    /**
     * Serialize StepProgress to JSON.
     */
    void to_json(nlohmann::json& j, const StepProgress& progress);

    /**
     * Deserialize StepProgress from JSON.
     */
    void from_json(const nlohmann::json& j, StepProgress& progress);

    /**
     * Serialize SequenceProgress to JSON.
     */
    void to_json(nlohmann::json& j, const SequenceProgress& progress);

    /**
     * Deserialize SequenceProgress from JSON.
     */
    void from_json(const nlohmann::json& j, SequenceProgress& progress);

    /**
     * Serialize Result to JSON.
     */
    void to_json(nlohmann::json& j, const Result& result);

    /**
     * Deserialize Result from JSON.
     */
    void from_json(const nlohmann::json& j, Result& result);

} // namespace aknet::startup::types

namespace aknet::startup {

    /**
     * JSON utility functions for startup types.
     */
    namespace json {

        // =========================================================================
        // Helper Functions
        // =========================================================================

        /**
         * Serialize SequenceProgress to a JSON string.
         *
         * @param progress Progress to serialize.
         *
         * @return JSON string representation.
         */
        std::string serialize_progress(const SequenceProgress& progress);

        /**
         * Serialize Result to a JSON string.
         *
         * @param result Result to serialize.
         *
         * @return JSON string representation.
         */
        std::string serialize_result(const Result& result);

        /**
         * Serialize StepConfig to a JSON string.
         *
         * @param config Config to serialize.
         *
         * @return JSON string representation.
         */
        std::string serialize_step_config(const StepConfig& config);

        /**
         * Deserialize SequenceProgress from a JSON string.
         *
         * @param json_str JSON string to parse.
         * @param[out] out SequenceProgress to populate.
         *
         * @return Result indicating success or parse error.
         */
        Result deserialize_progress(const std::string& json_str, SequenceProgress& out);

        /**
         * Deserialize Result from a JSON string.
         *
         * @param json_str JSON string to parse.
         * @param[out] out Result to populate.
         *
         * @return Result indicating success or parse error.
         */
        Result deserialize_result(const std::string& json_str, Result& out);

        /**
         * Deserialize StepConfig from a JSON string.
         *
         * @param json_str JSON string to parse.
         * @param[out] out StepConfig to populate.
         *
         * @return Result indicating success or parse error.
         */
        Result deserialize_step_config(const std::string& json_str, StepConfig& out);

    } // namespace json

    // Re-export helper functions at startup:: level for convenience
    using json::serialize_progress;
    using json::serialize_result;
    using json::serialize_step_config;
    using json::deserialize_progress;
    using json::deserialize_result;
    using json::deserialize_step_config;

} // namespace aknet::startup

#endif //AKNET_STARTUP_JSON_H
