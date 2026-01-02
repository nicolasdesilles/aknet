//
// Created by Nicolas Désilles on 02/01/2026.
//

#include "settings.h"

#include <filesystem>
#include <memory>
#include <ranges>
#include <string>

namespace aknet::settings {

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
    }

    void Settings::shutdown() {

        logger_ = nullptr;
        config_ = {};
    }

    std::filesystem::path Settings::path() {
        return config_.base_dir / config_.file_name;
    }

    std::shared_ptr<const AppSettings> Settings::snapshot() {
        // For now, returns the compile-time defaults as requested
        return std::make_shared<const AppSettings>(defaults_);
    }
}
