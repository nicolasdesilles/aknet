//
// Created by Nicolas Désilles on 08/01/2026.
//

#ifndef AKNET_STARTUP_JSON_H
#define AKNET_STARTUP_JSON_H

#pragma once

#include "startup.h"
#include <nlohmann/json.hpp>

namespace aknet::startup {

    // -------------------------------------------------------------------------
    // Enum Serialization
    // -------------------------------------------------------------------------

    void to_json(nlohmann::json& j, AppState state);
    void from_json(const nlohmann::json& j, AppState& state);

    void to_json(nlohmann::json& j, StepStatus status);
    void from_json(const nlohmann::json& j, StepStatus& status);

    void to_json(nlohmann::json& j, AbortReason reason);
    void from_json(const nlohmann::json& j, AbortReason& reason);

    // -------------------------------------------------------------------------
    // Struct Serialization
    // -------------------------------------------------------------------------

    void to_json(nlohmann::json& j, const StepConfig& config);
    void from_json(const nlohmann::json& j, StepConfig& config);

    void to_json(nlohmann::json& j, const StepProgress& progress);
    void from_json(const nlohmann::json& j, StepProgress& progress);

    void to_json(nlohmann::json& j, const SequenceProgress& progress);
    void from_json(const nlohmann::json& j, SequenceProgress& progress);

    void to_json(nlohmann::json& j, const Result& result);
    void from_json(const nlohmann::json& j, Result& result);

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    // Serialize to JSON string
    std::string serialize_progress(const SequenceProgress& progress);
    std::string serialize_result(const Result& result);
    std::string serialize_step_config(const StepConfig& config);

    // Deserialize from JSON string (returns Result to handle errors)
    Result deserialize_progress(const std::string& json_str, SequenceProgress& out);
    Result deserialize_result(const std::string& json_str, Result& out);
    Result deserialize_step_config(const std::string& json_str, StepConfig& out);

} // namespace aknet::startup

#endif //AKNET_STARTUP_JSON_H