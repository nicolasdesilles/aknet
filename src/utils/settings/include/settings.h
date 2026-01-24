/**
 * Persistent application settings module with staged editing.
 *
 * This util provides a centralized, thread-safe settings system for the app.
 * This util is meant to be owned by the core.
 *
 * Settings are stored as JSON and loaded/saved from disk.
 * The system uses a staged editing pattern where changes are made to a pending copy and explicitly saved.
 *
 * @info
 * The other modules in the application will not create their own settings instance.
 * Instead, the core will own the Settings instance and provide read access to modules via immutable snapshots.
 *
 * ## Design Goals
 *
 * - **Thread Safety**: Read access via immutable snapshots, write access protected by mutex.
 * - **Staged Editing**: Changes are staged in a pending copy, allowing preview and discard before saving.
 * - **Restart Impact**: Rules determine which settings changes require module or application restarts.
 * - **Atomic Persistence**: File writes use temp files and backups to prevent corruption.
 * - **JSON Format**: Human-readable settings file using nlohmann::json.
 *
 */

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
#include <vector>
#include <nlohmann/json.hpp>

namespace aknet::settings {

    // =========================================================================
    // App Settings Struct Definitions
    // =========================================================================

    /**
     * General application settings.
     *
     * Contains settings that affect the overall application behavior.
     */
    struct General {
        std::string log_level = "debug";  ///< Logging verbosity level (trace, debug, info, warn, error, critical, off)
        int test_restart_impact = 0;      ///< Test field for restart impact functionality
    };

    /**
     * Audio engine settings.
     *
     * Contains settings that affect audio processing.
     *
     */
    struct Audio {
        int sampling_rate = 48000;  ///< Sample rate in Hz
        int buffer_size = 256;      ///< Audio buffer size in samples
    };

    /**
     * Root application settings structure.
     *
     * Contains all application settings organized into logical groups.
     * This is the main structure that gets serialized to/from JSON.
     *
     * #### Schema Versioning
     *
     * The `schema_version` field tracks the settings file format version.
     * When the settings structure changes, increment the version and handle migration in load_or_create().
     */
    struct AppSettings {
        int schema_version = 1;  ///< Settings file format version for migration support
        General general;         ///< General application settings
        Audio audio;             ///< Audio engine settings
    };

    // JSON (de)serialization macros (required for: nlohmann::json j = settings;)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(General, log_level, test_restart_impact);
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Audio, sampling_rate, buffer_size);
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AppSettings, schema_version, general, audio);

    // =========================================================================
    // Restart Impact System
    // =========================================================================

    /**
     * Defines the restart behavior required when a specific setting changes.
     *
     * Restart rules are registered with the Settings instance to track which settings changes have side effects.
     * When save() is called, the system compares old and new values for each rule's key and reports the impact.
     *
     * #### Key Format
     *
     * Keys use dot notation to reference nested fields:
     *
     * - `"general.log_level"` - the log_level field in the general section
     * - `"audio.sampling_rate"` - the sampling_rate field in the audio section
     */
    struct RestartRule {
        std::string key;                        ///< Dot-notation path to the setting (e.g., "audio.sampling_rate")
        bool requires_app_restart = false;      ///< If true, changing this setting requires a full app restart
        std::string module_name_to_restart;     ///< Module that needs restart (empty if none or app restart required)
    };

    /**
     * Categorizes the type of restart needed after a settings change.
     */
    enum class RestartImpact {
        None,                   ///< No restart required
        ModuleRestartRequired,  ///< One or more modules need to be restarted
        AppRestartRequired      ///< Full application restart required
    };

    /**
     * Describes the restart impact of a save() operation.
     *
     * Returned by Settings::save() to inform the caller what actions are needed for changes to take effect.
     */
    struct SaveImpact {
        bool app_restart_required = false;                  ///< True if a full app restart is needed
        std::vector<std::string> modules_restart_required;  ///< List of module names that need restart
        std::vector<std::string> restart_sensitive_keys_changed;  ///< List of setting keys that triggered restart requirements
    };

    // =========================================================================
    // Configuration and Result Types
    // =========================================================================

    /**
     * Configuration for initializing the Settings system.
     */
    struct SettingsConfig {
        std::filesystem::path base_dir;               ///< Directory where settings file is stored
        std::string file_name = "aknet_settings.json";  ///< Settings file name
        int schema_version = 1;                       ///< Expected schema version for validation
    };

    /**
     * Simple result type for operations that can fail.
     *
     * Used throughout the settings system instead of exceptions for expected failure cases like file I/O errors.
     */
    struct Result {
        bool ok = true;      ///< True if the operation succeeded
        std::string error;   ///< Error message if ok is false
    };

    // =========================================================================
    // JSON Helper Functions
    // =========================================================================

    /**
     * Parse a JSON string into an AppSettings struct.
     *
     * @param json_str The JSON string to parse.
     * @param[out] out The AppSettings struct to populate.
     *
     * @return Result indicating success or failure with error message.
     */
    Result from_json_string(std::string_view json_str, AppSettings& out);

    /**
     * Serialize an AppSettings struct to a JSON string.
     *
     * @param settings The settings to serialize.
     *
     * @return JSON string representation of the settings.
     */
    std::string to_json_string(const AppSettings& settings);

    /**
     * Load AppSettings from a JSON file.
     *
     * @param file_path Path to the JSON file.
     * @param[out] out The AppSettings struct to populate.
     *
     * @return Result indicating success or failure with error message.
     */
    Result from_json_file(const std::filesystem::path& file_path, AppSettings& out);

    /**
     * Save AppSettings to a JSON file with atomic write.
     *
     * Uses a safe write pattern: writes to a temp file, backs up the existing file, then renames.
     * This prevents data loss if the write is interrupted.
     *
     * @param file_path Path to the JSON file.
     * @param settings The settings to save.
     *
     * @return Result indicating success or failure with error message.
     */
    Result to_json_file(const std::filesystem::path& file_path, const AppSettings& settings);

    // =========================================================================
    // Settings Class
    // =========================================================================

    /**
     * Thread-safe application settings manager with staged editing.
     *
     * Settings provides a centralized way to manage application configuration with the following features:
     *
     * - **Immutable snapshots** for thread-safe read access from any thread
     * - **Staged editing** where changes are made to a pending copy before saving
     * - **Restart impact tracking** to determine what needs restarting after changes
     * - **Atomic file persistence** to prevent corruption
     *
     * #### Lifecycle
     *
     * 1. Create a Settings instance
     * 2. Call init() with a logger and configuration
     * 3. Call load_or_create() to load existing settings or create defaults
     * 4. Use snapshot() to read settings, stage() to modify pending settings
     * 5. Call save() to persist changes
     * 6. Call shutdown() before destruction
     *
     * #### Thread Safety
     *
     * - snapshot() returns an immutable shared_ptr that can be safely read from any thread
     * - stage(), save(), and other mutating operations are protected by an internal mutex
     * - Multiple threads can hold snapshot references simultaneously
     *
     * #### Example
     *
     * ```cpp
     * auto settings = std::make_unique<Settings>();
     * settings->init(logger, config);
     * settings->load_or_create();
     *
     * // Read current settings (thread-safe)
     * auto snap = settings->snapshot();
     * int sample_rate = snap->audio.sampling_rate;
     *
     * // Modify settings (staged)
     * settings->stage([](AppSettings& s) {
     *     s.audio.sampling_rate = 96000;
     * });
     *
     * // Save changes and check impact
     * auto [result, impact] = settings->save();
     * if (impact.modules_restart_required.contains("audio")) {
     *     // Restart audio engine
     * }
     * ```
     */
    class Settings {
    public:
        /**
         * Default constructor.
         *
         * Creates an uninitialized Settings instance.
         * Call init() before using any other methods.
         */
        Settings();

        /**
         * Destructor.
         */
        ~Settings();

        /**
         * Initialize the settings system.
         *
         * Must be called before any other methods.
         * Sets up the logger, configuration, and creates an initial snapshot with default values.
         *
         * @param logger Logger instance for diagnostic output.
         * @param config Configuration specifying file location and schema version.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         * - Throws `std::invalid_argument` if config.base_dir is empty.
         */
        void init(std::shared_ptr<log::Logger> logger, SettingsConfig config);

        /**
         * Shutdown the settings system.
         *
         * Releases all resources and resets to uninitialized state.
         * Any unsaved pending changes are discarded.
         *
         * @warning
         * Does not save pending changes.
         * Call save() before shutdown() if you want to persist changes.
         */
        void shutdown();

        // -- State Queries --

        /**
         * Check if the settings system is initialized.
         *
         * @return True if init() has been called and shutdown() has not.
         */
        bool is_initialized();

        /**
         * Check if there are unsaved pending changes.
         *
         * Compares the pending settings against the active snapshot.
         *
         * @return True if pending settings differ from the active snapshot.
         */
        bool has_pending_changes();

        // -- Restart Rules --

        /**
         * Register a restart rule for a setting key.
         *
         * Restart rules define what happens when specific settings change.
         * When save() is called, it checks all registered rules against the changed keys.
         *
         * @param rule The restart rule to register.
         *
         * #### Example
         *
         * ```cpp
         * settings->add_restart_rule({
         *     .key = "audio.sampling_rate",
         *     .requires_app_restart = false,
         *     .module_name_to_restart = "audio"
         * });
         * ```
         */
        void add_restart_rule(RestartRule rule);

        /**
         * Get all registered restart rules.
         *
         * @return Copy of the restart rules vector.
         */
        std::vector<RestartRule> get_restart_rules();

        // -- File Path --

        /**
         * Get the full path to the settings file.
         *
         * @return Combined path of base_dir and file_name from configuration.
         */
        std::filesystem::path path();

        // -- Reading Settings --

        /**
         * Get an immutable snapshot of the current active settings.
         *
         * The returned pointer is thread-safe to read from any thread.
         * The snapshot remains valid even if the settings are modified and saved.
         *
         * @return Shared pointer to immutable AppSettings.
         *
         * @note
         * The snapshot is replaced when save() is called.
         * Callers holding old snapshots will still see the old values.
         */
        std::shared_ptr<const AppSettings> snapshot();

        // -- Loading Settings --

        /**
         * Load settings from file or create with defaults.
         *
         * If the settings file exists, loads and validates it.
         * If the file does not exist, creates it with default values.
         *
         * @return Result indicating success or failure.
         *
         * @note
         * This should be called after init() to load persisted settings.
         * On success, both snapshot and pending are updated to the loaded values.
         */
        Result load_or_create();

        // -- Staged Editing --

        /**
         * Get a mutable copy of the pending settings.
         *
         * @return Copy of the pending AppSettings struct.
         *
         * @note
         * This returns a copy, not a reference.
         * To modify pending settings, use stage() instead.
         */
        AppSettings pending_copy();

        /**
         * Modify the pending settings using a mutator function.
         *
         * The mutator receives a mutable reference to the pending settings.
         * Changes are not persisted until save() is called.
         *
         * @param mutator Function that modifies the pending settings.
         *
         * @return Result (currently always succeeds).
         *
         * #### Example
         *
         * ```cpp
         * settings->stage([](AppSettings& s) {
         *     s.general.log_level = "trace";
         *     s.audio.buffer_size = 512;
         * });
         * ```
         */
        Result stage(std::function<void(AppSettings&)> mutator);

        /**
         * Discard pending changes and reset to the active snapshot.
         *
         * @return Result indicating success or failure.
         */
        Result reset_pending_to_active();

        // -- Saving Settings --

        /**
         * Result of a save operation including restart impact information.
         */
        struct SaveResult {
            Result result;        ///< Success/failure status
            SaveImpact save_impact;  ///< What restarts are needed for changes to take effect
        };

        /**
         * Save pending settings to file and update the active snapshot.
         *
         * Writes the pending settings to disk using atomic file operations.
         * Compares old and new settings against restart rules to determine impact.
         * Updates the active snapshot to the newly saved values.
         *
         * @return SaveResult containing success status and restart impact information.
         *
         * #### Example
         *
         * ```cpp
         * auto [result, impact] = settings->save();
         * if (!result.ok) {
         *     logger->error("Failed to save: {}", result.error);
         *     return;
         * }
         * if (impact.app_restart_required) {
         *     // Prompt user to restart application
         * }
         * for (const auto& module : impact.modules_restart_required) {
         *     // Restart the affected module
         * }
         * ```
         */
        SaveResult save();

        // -- Import/Export --

        /**
         * Export current active settings to an arbitrary file.
         *
         * Useful for creating backups or sharing configurations.
         *
         * @param file_path Destination file path.
         *
         * @return Result indicating success or failure.
         */
        Result export_to_file(const std::filesystem::path& file_path);

        /**
         * Import settings from a file into pending.
         *
         * Loads settings from the specified file into the pending copy.
         * The imported settings are not active until save() is called.
         *
         * @param file_path Source file path.
         *
         * @return Result indicating success or failure.
         *
         * @note
         * This only updates pending settings, not the active snapshot.
         * Call save() after import to make the changes active.
         */
        Result import_from_file(const std::filesystem::path& file_path);

    private:
        /**
         * Protects pending_ and restart_rules_.
         */
        mutable std::mutex pending_mutex_;

        /**
         * Registered restart rules.
         */
        std::vector<RestartRule> restart_rules_;

        /**
         * Default settings (used for reset and initial creation).
         */
        AppSettings defaults_{};

        /**
         * Pending changes (staged but not yet saved).
         */
        AppSettings pending_{};

        /**
         * Immutable active settings snapshot.
         */
        std::shared_ptr<const AppSettings> snapshot_;

        /**
         * Configuration (file path, schema version).
         */
        SettingsConfig config_;

        /**
         * True if init() has been called.
         */
        bool initialized_ = false;

        /**
         * Logger for diagnostic output.
         */
        std::shared_ptr<log::Logger> logger_;
    };

}

#endif //AKNET_SETTINGS_H
