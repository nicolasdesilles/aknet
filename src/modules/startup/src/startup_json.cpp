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
        j = nlohmann::json{
            {"id", config.id},
            {"display_name", config.display_name},
            {"timeout", config.timeout.count()},
            {"critical", config.critical},
            {"can_skip", config.can_skip}
        };
    }

    void from_json(const nlohmann::json& j, StepConfig& config) {
        j.at("id").get_to(config.id);
        j.at("display_name").get_to(config.display_name);

        int timeout_seconds = j.at("timeout").get<int>();
        config.timeout = std::chrono::seconds{timeout_seconds};

        j.at("critical").get_to(config.critical);
        j.at("can_skip").get_to(config.can_skip);
    }

    void to_json(nlohmann::json& j, const StepProgress& progress) {
        j = nlohmann::json{
            {"id", progress.id},
            {"display_name", progress.display_name},
            {"status", progress.status},  // Uses enum serializer
            {"message", progress.message}
        };

        // Handle optional start_time
        if (progress.start_time.has_value()) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                progress.start_time.value().time_since_epoch()
            ).count();
            j["start_time"] = ms;
        } else {
            j["start_time"] = nullptr;
        }

        // Handle optional end_time
        if (progress.end_time.has_value()) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                progress.end_time.value().time_since_epoch()
            ).count();
            j["end_time"] = ms;
        } else {
            j["end_time"] = nullptr;
        }
    }

    void from_json(const nlohmann::json& j, StepProgress& progress) {
        j.at("id").get_to(progress.id);
        j.at("display_name").get_to(progress.display_name);
        j.at("status").get_to(progress.status);  // Uses enum deserializer
        j.at("message").get_to(progress.message);

        // Handle optional start_time
        if (j.at("start_time").is_null()) {
            progress.start_time = std::nullopt;
        } else {
            int64_t ms = j.at("start_time").get<int64_t>();
            progress.start_time = std::chrono::steady_clock::time_point{
                std::chrono::milliseconds{ms}
            };
        }

        // Handle optional end_time
        if (j.at("end_time").is_null()) {
            progress.end_time = std::nullopt;
        } else {
            int64_t ms = j.at("end_time").get<int64_t>();
            progress.end_time = std::chrono::steady_clock::time_point{
                std::chrono::milliseconds{ms}
            };
        }
    }

    void to_json(nlohmann::json& j, const SequenceProgress& progress) {
        j = nlohmann::json{
            {"state", progress.state},
            {"current_step_index", progress.current_step_index},
            {"steps", progress.steps},
            {"can_retry", progress.can_retry},
            {"abort_reason", progress.abort_reason}
        };

        // Handle optional last_error
        if (progress.last_error.has_value()) {
            j["last_error"] = progress.last_error.value();
        } else {
            j["last_error"] = nullptr;
        }
    }

    void from_json(const nlohmann::json& j, SequenceProgress& progress) {
        j.at("state").get_to(progress.state);
        j.at("current_step_index").get_to(progress.current_step_index);
        j.at("steps").get_to(progress.steps);
        j.at("can_retry").get_to(progress.can_retry);
        j.at("abort_reason").get_to(progress.abort_reason);

        // Handle optional last_error
        if (j.at("last_error").is_null()) {
            progress.last_error = std::nullopt;
        } else {
            progress.last_error = j.at("last_error").get<std::string>();
        }
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
        nlohmann::json j = progress;
        return j.dump();
    }

    std::string serialize_result(const Result& result) {
        nlohmann::json j = result;
        return j.dump();
    }

    std::string serialize_step_config(const StepConfig& config) {
        nlohmann::json j = config;
        return j.dump();
    }

    Result deserialize_progress(const std::string& json_str, SequenceProgress& out) {
        try {
            auto j = nlohmann::json::parse(json_str);
            out = j.get<SequenceProgress>();
            return {true, ""};
        } catch (const nlohmann::json::parse_error& e) {
            return {false, std::string("JSON parse error: ") + e.what()};
        } catch (const nlohmann::json::type_error& e) {
            return {false, std::string("JSON type error: ") + e.what()};
        } catch (const nlohmann::json::out_of_range& e) {
            return {false, std::string("JSON missing field: ") + e.what()};
        } catch (const std::exception& e) {
            return {false, std::string("Unexpected error: ") + e.what()};
        }
    }

    Result deserialize_result(const std::string& json_str, Result& out) {
        try {
            auto j = nlohmann::json::parse(json_str);
            out = j.get<Result>();
            return {true, ""};
        } catch (const nlohmann::json::parse_error& e) {
            return {false, std::string("JSON parse error: ") + e.what()};
        } catch (const nlohmann::json::type_error& e) {
            return {false, std::string("JSON type error: ") + e.what()};
        } catch (const nlohmann::json::out_of_range& e) {
            return {false, std::string("JSON missing field: ") + e.what()};
        } catch (const std::exception& e) {
            return {false, std::string("Unexpected error: ") + e.what()};
        }
    }

    Result deserialize_step_config(const std::string& json_str, StepConfig& out) {
        try {
            auto j = nlohmann::json::parse(json_str);
            out = j.get<StepConfig>();
            return {true, ""};
        } catch (const nlohmann::json::parse_error& e) {
            return {false, std::string("JSON parse error: ") + e.what()};
        } catch (const nlohmann::json::type_error& e) {
            return {false, std::string("JSON type error: ") + e.what()};
        } catch (const nlohmann::json::out_of_range& e) {
            return {false, std::string("JSON missing field: ") + e.what()};
        } catch (const std::exception& e) {
            return {false, std::string("Unexpected error: ") + e.what()};
        }
    }

} // namespace aknet::startup