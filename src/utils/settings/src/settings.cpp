//
// Created by Nicolas Désilles on 02/01/2026.
//

#include "settings.h"

#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <nlohmann/json.hpp>

namespace aknet::settings {

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    Result from_json_string(std::string_view json_str, AppSettings &out) {

        nlohmann::json j;

        // First we try to parse the string to JSON
        try {
            j = nlohmann::json::parse(json_str);
        }
        catch (nlohmann::json::parse_error& e) {
            return Result{.ok = false, .error = e.what()};
        }

        // Then we try to convert to our struct
        try {
            out = j.get<AppSettings>();
        }
        catch (nlohmann::json::type_error& e) {
            return Result{.ok = false, .error = e.what()};
        }

        return Result{.ok = true};;
    }

    std::string to_json_string(const AppSettings &settings) {

        nlohmann::json j = settings;

        return j.dump();
    }



    // -------------------------------------------------------------------------
    // Settings implementation
    // -------------------------------------------------------------------------

    // Constructor
    Settings::Settings() = default;

    // Destructor
    Settings::~Settings() = default;

    // Init
    void Settings::init(std::shared_ptr<log::Logger> logger, SettingsConfig config) {
        if (!logger) {
            throw std::invalid_argument("A valid Logger must be provided to Settings on creation");
        }

        if (config.base_dir.empty()) {
            throw std::invalid_argument("A valid base directory must be provided to Settings on creation");
        }

        if (config.file_name.empty()) {
            config.file_name = "aknet_settings.json";
        }

        logger_ = std::move(logger);
        config_ = std::move(config);
        initialized_ = true;
    }

    void Settings::shutdown() {

        logger_ = nullptr;
        config_ = {};
        initialized_ = false;
    }

    bool Settings::is_initialized() {
        return initialized_;
    }

    std::filesystem::path Settings::path() {
        return config_.base_dir / config_.file_name;
    }

    std::shared_ptr<const AppSettings> Settings::snapshot() {
        // For now, returns the compile-time defaults as requested
        return std::make_shared<const AppSettings>(defaults_);
    }
}
