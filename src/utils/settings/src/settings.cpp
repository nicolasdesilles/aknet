//
// Created by Nicolas Désilles on 02/01/2026.
//

#include "settings.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <memory>
#include <ranges>
#include <iostream>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>

namespace aknet::settings {

    namespace {
        bool key_changed(const std::string &key, const AppSettings &old_settings, const AppSettings &new_settings) {
            try {
                // Convert both settings to JSON
                nlohmann::json old_json = old_settings;
                nlohmann::json new_json = new_settings;

                // Parse the key path (e.g., "general.log_level" -> ["general", "log_level"])
                std::vector<std::string> key_parts;
                std::stringstream ss(key);
                std::string part;
                while (std::getline(ss, part, '.')) {
                    key_parts.push_back(part);
                }

                // Navigate to the nested value in both JSONs
                nlohmann::json old_value = old_json;
                nlohmann::json new_value = new_json;

                for (const auto &key_part: key_parts) {
                    if (!old_value.contains(key_part) || !new_value.contains(key_part)) {
                        // Key doesn't exist in one or both settings
                        return false;
                    }
                    old_value = old_value[key_part];
                    new_value = new_value[key_part];
                }

                // Compare the values
                return old_value != new_value;
        
            } catch (const nlohmann::json::exception&) {
                // If any JSON operation fails, assume no change
                return false;
            }
        }

        void add_unique(std::vector<std::string>& items, const std::string& value) {
            if (value.empty()) {
                return;
            }

            if (std::find(items.begin(), items.end(), value) == items.end()) {
                items.push_back(value);
            }
        }

        std::string join_strings(const std::vector<std::string>& items, std::string_view sep) {
            std::string out;
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i > 0) {
                    out += sep;
                }
                out += items[i];
            }
            return out;
        }
    }

    // -------------------------------------------------------------------------
    // Audio Settings Validation
    // -------------------------------------------------------------------------

    bool is_valid_sample_rate(int rate) {
        return std::find(VALID_SAMPLE_RATES.begin(), VALID_SAMPLE_RATES.end(), rate) != VALID_SAMPLE_RATES.end();
    }

    bool is_valid_buffer_size(int size) {
        return std::find(VALID_BUFFER_SIZES.begin(), VALID_BUFFER_SIZES.end(), size) != VALID_BUFFER_SIZES.end();
    }

    int find_nearest_sample_rate(int rate) {
        if (VALID_SAMPLE_RATES.empty()) {
            return 48000; // Fallback default
        }

        int nearest = VALID_SAMPLE_RATES[0];
        int min_diff = std::abs(rate - nearest);

        for (int valid_rate : VALID_SAMPLE_RATES) {
            int diff = std::abs(rate - valid_rate);
            if (diff < min_diff) {
                min_diff = diff;
                nearest = valid_rate;
            }
        }

        return nearest;
    }

    int find_nearest_buffer_size(int size) {
        if (VALID_BUFFER_SIZES.empty()) {
            return 256; // Fallback default
        }

        int nearest = VALID_BUFFER_SIZES[0];
        int min_diff = std::abs(size - nearest);

        for (int valid_size : VALID_BUFFER_SIZES) {
            int diff = std::abs(size - valid_size);
            if (diff < min_diff) {
                min_diff = diff;
                nearest = valid_size;
            }
        }

        return nearest;
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    Result from_json_string(std::string_view json_str, AppSettings &out) {

        nlohmann::json j;

        // First we try to parse the string to JSON
        try {
            j = nlohmann::json::parse(json_str);
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

        return Result{.ok = true};
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

        if (std::filesystem::exists(file_path) && std::filesystem::is_directory(file_path)) {
            return Result{
                .ok = false,
                .error = "Target path " + file_path.string() + " is a directory"};
        }

        std::filesystem::path parent_dir = file_path.parent_path();
        std::filesystem::create_directories(parent_dir);

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
        pending_ = *snapshot_;
        initialized_ = true;

        logger_->info("Settings system initialized.");

    }

    void Settings::shutdown() {

        logger_ = nullptr;
        config_ = {};
        snapshot_ = nullptr;
        pending_ = {};
        initialized_ = false;
    }

    bool Settings::is_initialized() {
        return initialized_;
    }

    bool Settings::has_pending_changes() {

        if (!snapshot_) {
            return false;
        }

        std::lock_guard lock(pending_mutex_);
        nlohmann::json pending_json = pending_;
        nlohmann::json snapshot_json = *snapshot_;
        return pending_json != snapshot_json;
    }

    void Settings::add_restart_rule(RestartRule rule) {
        std::lock_guard lock(pending_mutex_);
        restart_rules_.push_back(std::move(rule));
    }

    std::vector<RestartRule> Settings::get_restart_rules() {
        std::lock_guard lock(pending_mutex_);
        return restart_rules_;
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
            return Result{.ok = false, .error = "Settings system not initialized before load or create"};
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

            // Enforce validation on loaded values - auto-correct if needed
            bool corrected = false;
            
            if (!is_valid_sample_rate(loaded_settings.audio.sampling_rate)) {
                int old_rate = loaded_settings.audio.sampling_rate;
                loaded_settings.audio.sampling_rate = find_nearest_sample_rate(old_rate);
                logger_->warn("Corrected invalid sample rate {} Hz to {} Hz", old_rate, loaded_settings.audio.sampling_rate);
                corrected = true;
            }
            
            if (!is_valid_buffer_size(loaded_settings.audio.buffer_size)) {
                int old_size = loaded_settings.audio.buffer_size;
                loaded_settings.audio.buffer_size = find_nearest_buffer_size(old_size);
                logger_->warn("Corrected invalid buffer size {} samples to {} samples", old_size, loaded_settings.audio.buffer_size);
                corrected = true;
            }
            
            // If we corrected values, save the file immediately
            if (corrected) {
                Result save_result = to_json_file(settings_file_path, loaded_settings);
                if (save_result.ok) {
                    logger_->info("Saved corrected settings to file");
                } else {
                    logger_->warn("Could not save corrected settings: {}", save_result.error);
                }
            }

            snapshot_ = std::make_shared<const AppSettings>(loaded_settings);
            pending_ = *snapshot_;
            logger_->info("Settings loaded successfully");

        }
        else {

            logger_->info("Settings file {} does not exist, creating with defaults...", settings_file_path.string());

            Result export_result = to_json_file(settings_file_path, defaults_);

            if (!export_result.ok) {
                return Result{.ok = false, .error = "Failed to export default settings to file " + settings_file_path.string() + ": " + export_result.error};
            }

            snapshot_ = std::make_shared<const AppSettings>(defaults_);
            pending_ = *snapshot_;
            logger_->info("Settings loaded successfully from default values.");
        }

        return Result{.ok = true};

    }

    AppSettings Settings::pending_copy() {
        std::lock_guard lock(pending_mutex_);
        return pending_;
    }

    Result Settings::stage(std::function<void(AppSettings &)> mutator) {
        std::lock_guard lock(pending_mutex_);
        
        // Create a temporary copy to validate before applying
        AppSettings temp = pending_;
        mutator(temp);
        
        // Validate sample rate
        if (!is_valid_sample_rate(temp.audio.sampling_rate)) {
            std::ostringstream oss;
            oss << "Invalid sample rate: " << temp.audio.sampling_rate 
                << ". Must be one of: ";
            for (size_t i = 0; i < VALID_SAMPLE_RATES.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << VALID_SAMPLE_RATES[i];
            }
            return Result{.ok = false, .error = oss.str()};
        }
        
        // Validate buffer size
        if (!is_valid_buffer_size(temp.audio.buffer_size)) {
            std::ostringstream oss;
            oss << "Invalid buffer size: " << temp.audio.buffer_size 
                << ". Must be one of: ";
            for (size_t i = 0; i < VALID_BUFFER_SIZES.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << VALID_BUFFER_SIZES[i];
            }
            return Result{.ok = false, .error = oss.str()};
        }
        
        // If validation passes, apply the changes
        pending_ = temp;
        return Result{.ok = true};
    }

    Result Settings::reset_pending_to_active() {
        if (!initialized_ || !snapshot_) {
            return Result{.ok = false, .error = "Settings not initialized"};
        }
        std::lock_guard lock(pending_mutex_);
        pending_ = *snapshot_;;
        return Result{.ok = true};
    }

    Settings::SaveResult Settings::save() {

        SaveImpact impact;

        if (!initialized_) {
            return SaveResult{.result = Result{.ok = false, .error = "Settings not initialized before save"}, .save_impact = impact};
        }

        logger_->info("Saving pending settings changes...");

        const AppSettings old_settings = *snapshot_;

        AppSettings to_save;
        std::vector<RestartRule> restart_rules_copy;
        {
            std::lock_guard lock(pending_mutex_);
            to_save = pending_;
            restart_rules_copy = restart_rules_;
        }


        Result write_result = to_json_file(path(), to_save);

        if (!write_result.ok) {
            logger_->error("Failed to save settings to file {}: {}", path().string(), write_result.error);
            return SaveResult{.result = Result{.ok = false, .error = "Failed to save settings to file " + path().string() + ": " + write_result.error}, .save_impact = impact};
        }

        // Compute impact based on manual restart rules list.

        for (const auto& rule : restart_rules_copy) {
            if (!key_changed(rule.key, old_settings, to_save)) {
                continue;
            }

            add_unique(impact.restart_sensitive_keys_changed, rule.key);

            if (rule.requires_app_restart) {
                impact.app_restart_required = true;
            }

            if (!rule.module_name_to_restart.empty()) {
                add_unique(impact.modules_restart_required, rule.module_name_to_restart);
            }
        }

        {
            std::lock_guard lock(pending_mutex_);
            snapshot_ = std::make_shared<const AppSettings>(to_save);
            pending_ = to_save;
        }

        logger_->info("Settings saved successfully.");

        if (!impact.restart_sensitive_keys_changed.empty()) {
            logger_->warn(
                "Restart-sensitive settings changed: {}",
                join_strings(impact.restart_sensitive_keys_changed, ", ")
            );
        }

        if (!impact.modules_restart_required.empty()) {
            logger_->warn(
                "Module restart required for: {}",
                join_strings(impact.modules_restart_required, ", ")
            );
        }

        if (impact.app_restart_required) {
            logger_->warn("Application restart required for settings to take effect.");
        }

        return SaveResult{.result = Result{.ok = true}, .save_impact = impact};

    }

    Result Settings::export_to_file(const std::filesystem::path &file_path) {

        if (!initialized_) {
            return Result{.ok = false, .error = "Settings not initialized before export"};
        }

        logger_->info("Exporting current settings to file...");

        auto current_settings = *snapshot_;

        Result export_result = to_json_file(file_path, current_settings);

        if (!export_result.ok) {
            logger_->error("Failed to export settings to file {}: {}", file_path.string(), export_result.error);
            return Result{.ok = false, .error = "Failed to export settings to file " + file_path.string() + ": " + export_result.error};
        }

        logger_->info("Settings exported successfully.");

        return Result{.ok = true};

    }

    Result Settings::import_from_file(const std::filesystem::path &file_path) {

        if (!initialized_) {
            return Result{.ok = false, .error = "Settings not initialized before export"};
        }

        logger_->info("Importing settings from file... : {}", file_path.string());

        AppSettings imported = defaults_;

        Result import_result = from_json_file(file_path, imported);

        if (!import_result.ok) {
            logger_->error("Failed to import settings from file {}: {}", file_path.string(), import_result.error);
            return Result{.ok = false, .error = "Failed to import settings from file " + file_path.string() + ": " + import_result.error};
        }

        std::lock_guard lock(pending_mutex_);
        pending_ = imported;

        logger_->info("Settings imported successfully into pending changes.");

        return Result{.ok = true};

    }
}
