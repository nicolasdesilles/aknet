//
// Created by Nicolas Désilles on 08/01/2026.
//

#include "startup_json.h"
#include <chrono>

namespace aknet::startup {

    // -------------------------------------------------------------------------
    // Enum Serialization
    // -------------------------------------------------------------------------

    void to_json(nlohmann::json& j, AppState state) {
        // ReSharper disable once CppRedundantCastExpression
        j = static_cast<int>(state);
    }

    void from_json(const nlohmann::json& j, AppState& state) {
        int value = j.get<int>();

        if (value < 0 or value > 3) {
            throw std::invalid_argument("Invalid AppState value: " + std::to_string(value));
        }

        state = static_cast<AppState>(value);
    }

    void to_json(nlohmann::json& j, StepStatus status) {
        // ReSharper disable once CppRedundantCastExpression
        j = static_cast<int>(status);
    }

    void from_json(const nlohmann::json& j, StepStatus& status) {
        int value = j.get<int>();

        if (value < 0 or value > 6) {
            throw std::invalid_argument("Invalid StepStatus value: " + std::to_string(value));
        }

        status = static_cast<StepStatus>(value);
    }

    void to_json(nlohmann::json& j, AbortReason reason) {
        // ReSharper disable once CppRedundantCastExpression
        j = static_cast<int>(reason);
    }

    void from_json(const nlohmann::json& j, AbortReason& reason) {
        int value = j.get<int>();

        if (value < 0 or value > 4) {
            throw std::invalid_argument("Invalid AbortReason value: " + std::to_string(value));
        }

        reason = static_cast<AbortReason>(value);
    }

    // -------------------------------------------------------------------------
    // Struct Serialization
    // -------------------------------------------------------------------------

    void to_json(nlohmann::json& j, const StepConfig& config) {
        // TODO: Implement
    }

    void from_json(const nlohmann::json& j, StepConfig& config) {
        // TODO: Implement
    }

    void to_json(nlohmann::json& j, const StepProgress& progress) {
        // TODO: Implement
    }

    void from_json(const nlohmann::json& j, StepProgress& progress) {
        // TODO: Implement
    }

    void to_json(nlohmann::json& j, const SequenceProgress& progress) {
        // TODO: Implement
    }

    void from_json(const nlohmann::json& j, SequenceProgress& progress) {
        // TODO: Implement
    }

    void to_json(nlohmann::json& j, const Result& result) {
        j = nlohmann::json{
            {"ok", result.ok},
            {"error", result.error}
        };
    }

    void from_json(const nlohmann::json& j, Result& result) {
        j.at("ok").get_to(result.ok);
        j.at("error").get_to(result.error);
    }

    // -------------------------------------------------------------------------
    // Helper Functions
    // -------------------------------------------------------------------------

    std::string serialize_progress(const SequenceProgress& progress) {
        // TODO: Implement
        return "";
    }

    std::string serialize_result(const Result& result) {
        // TODO: Implement
        return "";
    }

    std::string serialize_step_config(const StepConfig& config) {
        // TODO: Implement
        return "";
    }

    Result deserialize_progress(const std::string& json_str, SequenceProgress& out) {
        // TODO: Implement
        return {false, "Not implemented"};
    }

    Result deserialize_result(const std::string& json_str, Result& out) {
        // TODO: Implement
        return {false, "Not implemented"};
    }

    Result deserialize_step_config(const std::string& json_str, StepConfig& out) {
        // TODO: Implement
        return {false, "Not implemented"};
    }

} // namespace aknet::startup