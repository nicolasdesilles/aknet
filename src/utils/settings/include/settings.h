//
// Created by Nicolas Désilles on 02/01/2026.
//

#ifndef AKNET_SETTINGS_H
#define AKNET_SETTINGS_H

#pragma once

#include "logger.h"
#include <nlohmann/json.hpp>

namespace aknet::settings {
    // -------------------------------------------------------------------------
    // App settings structs definitions
    // -------------------------------------------------------------------------

    struct General {
        std::string log_level = "debug";
    };

    struct Audio {
        int sampling_rate = 48000;
        int buffer_size = 256;
    };

    struct AppSettings {
        int schema_version = 1;
        General general;
        Audio audio;
    };

    // JSON (de)serialization (required for: nlohmann::json j = settings;)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(General, log_level);
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Audio, sampling_rate, buffer_size);
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AppSettings, schema_version, general, audio);

    // -------------------------------------------------------------------------
    // Settings system
    // -------------------------------------------------------------------------

    struct SettingsConfig {
        std::filesystem::path base_dir;     // same as logs dir for now
        std::string file_name = "aknet_settings.json";
        int schema_version = 1;
    };

    struct Result {
        bool ok = true;
        std::string error;
    };

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    Result from_json_string(std::string_view json_str,AppSettings& out);
    std::string to_json_string(const AppSettings& settings);

    // -------------------------------------------------------------------------
    // Settings class
    // -------------------------------------------------------------------------

    class Settings {
    public:
        Settings();
        ~Settings();

        void init(std::shared_ptr<log::Logger> logger,
            SettingsConfig config);

        void shutdown();

        bool is_initialized();

        std::filesystem::path path();

        // Read (active snapshot)
        std::shared_ptr<const AppSettings> snapshot();

    private:
        AppSettings defaults_{}; // compile-time defaults
        SettingsConfig config_;

        bool initialized_ = false;

        std::shared_ptr<log::Logger> logger_;
    };


}

#endif //AKNET_SETTINGS_H