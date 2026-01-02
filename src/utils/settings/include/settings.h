//
// Created by Nicolas Désilles on 02/01/2026.
//

#ifndef AKNET_SETTINGS_H
#define AKNET_SETTINGS_H

#pragma once

#include "logger.h"

namespace aknet::settings {
    // -------------------------------------------------------------------------
    // App settings structs definitions
    // -------------------------------------------------------------------------

    struct General {
        std::string log_level = "info";
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
    // Settings class
    // -------------------------------------------------------------------------

    class Settings {
    public:
        Settings();
        ~Settings();

        void init(std::shared_ptr<log::Logger> logger,
            SettingsConfig config);

        void shutdown();

        std::filesystem::path path();

        // Read (active snapshot)
        std::shared_ptr<const AppSettings> snapshot();

    private:
        AppSettings defaults_{}; // compile-time defaults
        SettingsConfig config_;

        std::shared_ptr<log::Logger> logger_;
    };


}

#endif //AKNET_SETTINGS_H