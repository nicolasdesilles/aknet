//
// core.h - Central orchestrator for the aknet application.
//
// The core module owns and manages all other modules, providing a single entry
// point for application lifecycle management. See the `core` class documentation
// for details on responsibilities, initialization order, and usage.
//

#ifndef AKNET_CORE_H
#define AKNET_CORE_H

#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

// aknet utils and modules
#include <logger.h>
#include <settings.h>
#include <startup_manager.h>
#include <jack_module.h>
#include <audio.h>

// Saucer webview
#include <saucer/smartview.hpp>

namespace aknet::bridge {
    class EventBridge;
}

namespace aknet {

    /**
     * Configuration for core initialization.
     *
     * All paths are optional - defaults are used if not specified.
     */
    struct core_config {
        std::filesystem::path log_dir = {};       ///< Directory for log files (default: platform-specific)
        std::filesystem::path settings_dir = {};  ///< Directory for settings file (default: platform-specific)
        int settings_schema_version = 1;          ///< Settings schema version for migrations
        log::LogLevel log_level = log::LogLevel::info;  ///< Initial log level (may be overridden by saved settings)
    };

    /**
     * Central orchestrator for the aknet application.
     *
     * The core class owns all modules and manages their lifecycle. It provides
     * high-level methods for controlling the application and accessing module functionality.
     *
     * #### Module Ownership
     *
     * | Module | Lifetime | Access |
     * |--------|----------|--------|
     * | Logger | Core construction → destruction | Via log::get() |
     * | Settings | Core construction → destruction | Via settings() |
     * | StartupManager | Core construction → destruction | Via startup_manager() |
     * | EventBridge | init_bridge() → destruction | Internal only |
     *
     * #### Thread Safety
     *
     * Core methods should be called from the main thread. The owned modules
     * have their own thread safety guarantees (e.g., StartupManager is thread-safe).
     *
     * #### Example
     *
     * ```cpp
     * aknet::core app;
     *
     * // Access settings
     * auto snapshot = app.settings().snapshot();
     * int sample_rate = snapshot->audio.sampling_rate;
     *
     * // Control startup
     * app.start_startup();
     *
     * // Handle user cancel
     * app.abort_startup();
     *
     * // Retry after failure
     * if (app.startup_manager().can_retry()) {
     *     app.retry_startup();
     * }
     * ```
     */
    class core {
    public:
        /**
         * Construct and initialize the core.
         *
         * Initializes all subsystems in the correct order:
         * logging → settings → startup manager.
         *
         * @param config Configuration options. All fields have sensible defaults.
         *
         * #### Exceptions
         *
         * May throw if critical initialization fails (e.g., cannot create log directory).
         *
         * #### Example
         *
         * ```cpp
         * // Use defaults
         * aknet::core app;
         *
         * // Or with custom config
         * aknet::core app({
         *     .log_level = log::LogLevel::debug
         * });
         * ```
         */
        explicit core(const core_config& config = {});

        /**
         * Destructor.
         *
         * Shuts down all modules in reverse initialization order.
         * Ensures clean disconnection of bridge and proper resource cleanup.
         */
        ~core();

        // Non-copyable, non-movable
        core(const core&) = delete;
        core& operator=(const core&) = delete;

        /**
         * Temporary test function for development.
         *
         * @warning
         * Will be removed before release.
         */
        void test_function();

        /**
         * Get the startup manager.
         *
         * @return Reference to the StartupManager for advanced control.
         *
         * @note
         * Prefer using start_startup(), abort_startup(), retry_startup() for common operations.
         */
        startup::StartupManager& startup_manager() { return *startup_manager_; }

        /**
         * Get the startup manager (const).
         *
         * @return Const reference for read-only access.
         */
        const startup::StartupManager& startup_manager() const { return *startup_manager_; }

        /**
         * Initialize the event bridge with a webview.
         *
         * Creates the EventBridge and connects it to the StartupManager's events.
         * Must be called after the webview is created but before start_startup().
         *
         * @tparam WebviewT Type with an `execute(const std::string&)` method.
         * @param webview Pointer to the webview. Must not be null and must remain valid.
         *
         * @warning Can only be called once. Subsequent calls log a warning and return.
         *
         * #### Example
         *
         * ```cpp
         * saucer::smartview webview;
         * // Configure webview...
         * app.init_bridge(&webview);
         * ```
         */
        template<typename WebviewT>
        void init_bridge(WebviewT* webview);

        // =====================================================================
        // Startup Control (exposed to UI)
        // =====================================================================

        /**
         * Start the startup sequence.
         *
         * Begins asynchronous execution of the registered startup steps.
         * Progress is reported via the EventBridge to the frontend.
         *
         * @return true if startup was initiated, false if already running or no steps.
         *
         * #### Example
         *
         * ```cpp
         * if (!app.start_startup()) {
         *     // Handle error - maybe already running
         * }
         * ```
         */
        bool start_startup();

        /**
         * Abort the running startup sequence.
         *
         * Requests cancellation of the current startup sequence with AbortReason::UserRequested.
         * The abort is processed asynchronously.
         *
         * @note
         * Safe to call even if not currently running (no-op).
         */
        void abort_startup();

        /**
         * Retry the startup sequence after a failure.
         *
         * Can only be called after a critical step failure (when can_retry is true).
         * Resets all steps and runs the sequence again.
         *
         * @return true if retry was initiated, false if cannot retry.
         *
         * #### Example
         *
         * ```cpp
         * // After receiving startup:completed with success=false
         * if (app.startup_manager().can_retry()) {
         *     app.retry_startup();
         * }
         * ```
         */
        bool retry_startup();

        /**
         * Set the startup test mode.
         *
         * Configures different step sequences for testing various scenarios.
         * Useful for development and testing the UI's error handling.
         *
         * @param mode Test mode:
         *             - 0: Normal (default steps)
         *             - 1: With failure (includes a FailingStep)
         *             - 2: With timeout (includes a SlowStep that times out)
         *
         * @note
         * Clears current steps and replaces with the test sequence.
         *
         * #### Example
         *
         * ```cpp
         * // Test failure handling in UI
         * app.set_test_mode(1);
         * app.start_startup();  // Will fail at FailingStep
         * ```
         */
        void set_test_mode(int mode);

        // =====================================================================
        // Settings Access
        // =====================================================================

        /**
         * Get the settings manager.
         *
         * @return Reference to the Settings for reading and modifying settings.
         *
         * #### Example
         *
         * ```cpp
         * // Read current settings
         * auto snapshot = app.settings().snapshot();
         * std::cout << "Sample rate: " << snapshot->audio.sampling_rate << "\n";
         *
         * // Modify settings
         * app.settings().modify([](auto& s) {
         *     s.audio.sampling_rate = 96000;
         * });
         * auto result = app.settings().save();
         * ```
         */
        settings::Settings& settings() { return settings_; }

        /**
         * Get the settings manager (const).
         *
         * @return Const reference for read-only access.
         */
        const settings::Settings& settings() const { return settings_; }

        // =====================================================================
        // Settings Bridge (exposed to UI)
        // =====================================================================

        /**
         * Get current settings as JSON string.
         *
         * Returns a JSON representation of the active settings snapshot.
         * Thread-safe.
         *
         * @return JSON string of current AppSettings.
         */
        std::string get_settings_json();

        /**
         * Get pending settings as JSON string.
         *
         * Returns a JSON representation of staged (unsaved) settings.
         * Thread-safe.
         *
         * @return JSON string of pending AppSettings.
         */
        std::string get_pending_settings_json();

        /**
         * Stage a settings change from JSON.
         *
         * Parses the JSON string and applies it to pending settings.
         * Changes are not persisted until save_settings() is called.
         *
         * @param json_str JSON string representing AppSettings or a partial update.
         * @return JSON string of Result {ok, error}.
         *
         */
        std::string stage_settings_json(const std::string& json_str);

        /**
         * Save pending settings to disk.
         *
         * Persists staged changes and computes restart impact.
         *
         * @return JSON string of SaveResult {result: {ok, error}, save_impact: {...}}.
         */
        std::string save_settings_json();

        /**
         * Reset pending settings to active snapshot.
         *
         * Discards all staged changes.
         *
         * @return JSON string of Result {ok, error}.
         */
        std::string reset_pending_settings_json();

        /**
         * Check if there are unsaved pending changes.
         *
         * @return true if pending differs from active snapshot.
         */
        bool has_pending_settings_changes();

        /**
         * Get available audio devices as JSON.
         *
         * Returns a JSON array of AudioDevice objects with properties:
         * - id: device identifier
         * - name: human-readable name
         * - input_channels: number of input channels
         * - output_channels: number of output channels
         * - is_default: true if this is the system default device
         *
         * @return JSON array string of AudioDevice objects.
         */
        std::string get_audio_devices_json();

        /**
         * Get the system default audio device as JSON.
         *
         * Returns a JSON object representing the current system default device.
         *
         * @return JSON object string of AudioDevice.
         */
        std::string get_default_audio_device_json();

        /**
         * Get application status snapshot as JSON string.
         *
         * Includes app state, JACK server/client status, and audio settings.
         *
         * @return JSON string of StatusSnapshot.
         */
        std::string get_status_snapshot_json();


        /**
         * Retrieves the current audio levels.
         *
         * @return A list of audio levels, where each value corresponds to the respective channel's level.
         *
         * @see jack::JackAudioProcessor
         */
        std::vector<float> get_audio_levels();

        /**
         * Retrieves the peak levels of the given audio signal.
         *
         * @return A list of peak levels, where each value corresponds to the highest amplitude detected in a channel.
         *
         * @see jack::JackAudioProcessor
         */
        std::vector<float> get_peak_levels();

        /**
         * Reset the peak level values.
         */
        void reset_peak_levels();

    private:
        std::shared_ptr<log::Logger> logger_;

        void log_aknet_start_message();
        void start_status_tick();
        void stop_status_tick();

        /**
         * Create StepContext for startup steps.
         */
        startup::StepContext create_step_context();

        // Settings system
        settings::Settings settings_;

        // Owned modules
        std::unique_ptr<startup::StartupManager> startup_manager_;
        std::unique_ptr<bridge::EventBridge> bridge_;
        std::shared_ptr<jack::JackModule> jack_module_;
        std::shared_ptr<audio::AudioModule> audio_module_;

        // Status tick loop
        std::atomic<bool> status_tick_stop_{false};
        std::thread status_tick_thread_;
    };

} // namespace aknet

#endif // AKNET_CORE_H