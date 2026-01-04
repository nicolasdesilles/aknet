//
// Created by Nicolas Désilles on 02/01/2026.
//

#include "settings.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <ranges>
#include <iostream>
#include <sstream>
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
        catch (nlohmann::json::exception& e) {
            return Result{.ok = false, .error = e.what()};
        }

        // Then we try to convert to our struct
        try {
            out = j.get<AppSettings>();
        }
        catch (nlohmann::json::exception& e) {
            return Result{.ok = false, .error = e.what()};
        }

        return Result{.ok = true};;
    }

    std::string to_json_string(const AppSettings &settings) {

        nlohmann::json j = settings;

        return j.dump();
    }

    Result from_json_file(const std::filesystem::path &file_path, AppSettings &out) {
        
        std::ifstream input_stream(file_path);
        if (!input_stream.is_open()) {
            return Result{.ok = false, .error = "File " + file_path.string() + " does not exist"};
        }

        const std::string file_contents((std::istreambuf_iterator(input_stream)),std::istreambuf_iterator<char>());

        Result import_result = from_json_string(file_contents, out);

        if (!import_result.ok) {
            return Result{
            .ok = false,
            .error = "Failed to import settings from file " + file_path.string() + ": " + import_result.error};
        }

        return Result{.ok = true};
    }

    Result to_json_file(const std::filesystem::path &file_path, const AppSettings &settings) {

        std::filesystem::path parent_dir;
        try {
            parent_dir = file_path.parent_path();
        }
        catch (std::filesystem::filesystem_error& e) {
            return Result{
                .ok = false,
                .error = "Error when processing destination file path : " + file_path.string() +  e.what()};
        }

        try {
            std::filesystem::create_directories(parent_dir);
        }
        catch (std::filesystem::filesystem_error& e) {
            return Result{
                .ok = false,
                .error = "Error when creating parent directories for destination file path : " + file_path.string() +  e.what()};
        }

        std::filesystem::path tmp_path = file_path;
        tmp_path += ".tmp";

        std::string json_str;
        try {
            json_str = to_json_string(settings);
        }
        catch (std::exception& e) {
            return Result{
                .ok = false,
                .error = e.what()};
        }

        std::ofstream output_stream(tmp_path);

        if (!output_stream.is_open()) {
            return Result{
                .ok = false,
                .error = "Error when opening tmp file " + tmp_path.string() + " for writing"};
        }

        output_stream << json_str;
        output_stream.close();

        if (exists(file_path)) {
            const std::filesystem::path bak_path = file_path.string() + ".bak";

            if (exists(bak_path)) {
                try {
                    std::filesystem::remove(bak_path);
                }
                catch (std::filesystem::filesystem_error& e) {
                    return Result{
                        .ok = false,
                        .error = "Error when removing existing backup file " + bak_path.string() + ": " + e.what()};
                }
            }

            try {
                std::filesystem::rename(file_path, bak_path);
            }
            catch (std::filesystem::filesystem_error& e) {
                return Result{
                    .ok = false,
                    .error = "Error when renaming existing file " + file_path.string() + " with .bak: " + e.what()};
            }

            try {
                std::filesystem::rename(tmp_path, file_path);
            }
            catch (std::filesystem::filesystem_error& e) {
                // Best-effort rollback: restore the previous file.
                try {
                    if (exists(bak_path) && !exists(file_path)) {
                        std::filesystem::rename(bak_path, file_path);
                    }
                }
                catch (...) {
                    return Result{
                        .ok = false,
                        .error = "Error when renaming tmp file to destination file " + file_path.string() + ": " + e.what() + ", and could not restore .bak file"};
                }

                return Result{
                    .ok = false,
                    .error = "Error when renaming tmp file to destination file " + file_path.string() + ": " + e.what()};
            }

            try {
                std::filesystem::remove(bak_path);
            }
            catch (std::filesystem::filesystem_error& e) {
                return Result{
                    .ok = false,
                    .error = "Error when removing backup file " + bak_path.string() + ": " + e.what()};
            }
        } else {
            try {
                std::filesystem::rename(tmp_path, file_path);
            }
            catch (std::filesystem::filesystem_error& e) {
                return Result{
                    .ok = false,
                    .error = "Error when renaming tmp file to destination file " + file_path.string() + ": " + e.what()};
            }
        }

        return Result{.ok = true};
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
        snapshot_ = std::make_shared<const AppSettings>(defaults_);
        initialized_ = true;

        logger_->info("Settings system initialized.");

    }

    void Settings::shutdown() {

        logger_ = nullptr;
        config_ = {};
        snapshot_ = nullptr;
        initialized_ = false;
    }

    bool Settings::is_initialized() {
        return initialized_;
    }

    std::filesystem::path Settings::path() {
        return config_.base_dir / config_.file_name;
    }

    std::shared_ptr<const AppSettings> Settings::snapshot() {
        return snapshot_;
    }

    Result Settings::load_or_create() {

        auto settings_file_path = path();

        if (!initialized_) {
            return Result{.ok = false, .error = "Settings not initialized before load or create"};
        }

        if (config_.base_dir.empty()) {
            return Result{.ok = false, .error = "Settings base directory not set"};
        }

        if (settings_file_path.empty()) {
            return Result{.ok = false, .error = "Settings file path not set"};
        }

        try {
            std::filesystem::create_directories(settings_file_path.parent_path());
        }
        catch (std::filesystem::filesystem_error& e) {
            logger_->error("On settings load_or_create, could not create settings file parent directory: {}", e.what());
            return Result{.ok = false, .error = e.what()};
        }

        if (exists(settings_file_path)) {

            logger_->info("Settings file {} exists, loading...", settings_file_path.string());

            AppSettings loaded_settings;

            Result load_result = from_json_file(settings_file_path, loaded_settings);
            if (!load_result.ok) {
                logger_->error("Failed to load settings from file {}: {}", settings_file_path.string(), load_result.error);
                return Result{.ok = false, .error = "Failed to load settings from file " + settings_file_path.string() + ": " + load_result.error};
            }

            // Schema version check
            if (loaded_settings.schema_version != config_.schema_version) {
                logger_->warn("Settings file {} schema version mismatch ({} != {})", settings_file_path.string(), loaded_settings.schema_version, defaults_.schema_version);
            }
            snapshot_ = std::make_shared<const AppSettings>(loaded_settings);
            logger_->info("Settings loaded successfully");

        }
        else {

            logger_->info("Settings file {} does not exist, creating with defaults...", settings_file_path.string());

            Result export_result = to_json_file(settings_file_path, defaults_);

            if (!export_result.ok) {
                return Result{.ok = false, .error = "Failed to export default settings to file " + settings_file_path.string() + ": " + export_result.error};
            }

            snapshot_ = std::make_shared<const AppSettings>(defaults_);
            logger_->info("Settings loaded successfully from default values.");
        }

        return Result{.ok = true};

    }
}
