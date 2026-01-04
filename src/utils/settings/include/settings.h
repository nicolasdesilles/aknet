//
// Created by Nicolas Désilles on 02/01/2026.
//

#ifndef AKNET_SETTINGS_H
#define AKNET_SETTINGS_H

#pragma once

#include "logger.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
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

    Result from_json_file(const std::filesystem::path& file_path,AppSettings& out);
    Result to_json_file(const std::filesystem::path& file_path,const AppSettings& settings);


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

        // Helpers
        bool is_initialized();
        bool has_pending_changes();

        std::filesystem::path path();

        // Read (active snapshot)
        std::shared_ptr<const AppSettings> snapshot();

        // Load settings from file and if file does not exist at location create one with default values
        Result load_or_create();

        // Staged editing
        AppSettings pending_copy();
        Result stage(std::function<void(AppSettings&)> mutator);
        Result reset_pending_to_active();

        // Save
        Result save();

        // File Import/Export
        Result export_to_file(const std::filesystem::path& file_path);
        Result import_from_file(const std::filesystem::path& file_path);

    private:
        mutable std::mutex pending_mutex_;

        AppSettings defaults_{};
        AppSettings pending_{};

        std::shared_ptr<const AppSettings> snapshot_;
        SettingsConfig config_;

        bool initialized_ = false;

        std::shared_ptr<log::Logger> logger_;
    };


}

#endif //AKNET_SETTINGS_H