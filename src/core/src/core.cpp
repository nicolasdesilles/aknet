//
// Created by Nicolas Désilles on 27/12/2025.
//

#include "core.h"
#include <version.h>
#include <bridge.h>
#include <saucer/smartview.hpp>
#include <steps_definition.h>

namespace aknet {

    // Constructor
    core::core(const core_config& config) {

        // Initialize logging infrastructure (the core owns it)
        log::init(config.log_dir);
        log::set_global_log_level(config.log_level);

        // Get a logger for the core
        logger_ = log::get("core");

        log_aknet_start_message();
        logger_->info("Initializing Core...");

        // Start the settings system
        auto settings_config = settings::SettingsConfig{
        .base_dir = config.settings_dir,
        .schema_version = config.settings_schema_version};
        
        auto settings_logger = log::get("settings");
        
        settings_.init(settings_logger, settings_config);
        
        // Loading saved settings
        settings_.load_or_create();

        // Restart rules for settings changes
        settings_.add_restart_rule({
            .key = "audio.num_channels",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });
        settings_.add_restart_rule({
            .key = "audio.sampling_rate",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });
        settings_.add_restart_rule({
            .key = "audio.buffer_size",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });
        settings_.add_restart_rule({
            .key = "jack.client_name",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });

        // Applying log level stored in settings
        log::set_global_log_level(log::string_to_log_level(settings_.snapshot()->general.log_level));

        // Startup manager init
        auto startup_logger = log::get("startup");

        // Create a shared_ptr with a no-op deleter since core owns settings_
        auto settings_ptr = std::shared_ptr<settings::Settings>(
            &settings_,
            [](settings::Settings*){} // no-op deleter
        );

        startup_manager_ = std::make_unique<startup::StartupManager>(
            startup_logger,
            settings_ptr
        );

        // Register fake startup steps for testing
        startup_manager_->set_steps(startup::create_default_steps());
        logger_->info("Registered {} startup steps", 6);

        logger_->info("Initializing Core: Done.");
    }

    // Destructor
    core::~core() {
        logger_->info("Core shutting down...");

        // Destroy modules
        if (bridge_) {
            bridge_->disconnect();
        }
        bridge_.reset();

        startup_manager_.reset();
        
        // Shutdown settings system
        settings_.shutdown();

        // Release our logger before shutting down logging system
        logger_.reset();

        // Shutdown logging infrastructure last
        log::shutdown();
    }

    void core::test_function() {
        logger_->info("Core test function called");
    }

    template<typename WebviewT>
    void core::init_bridge(WebviewT* webview) {
        if (bridge_) {
            logger_->warn("Bridge already initialized");
            return;
        }

        auto bridge_logger = log::get("bridge");
        bridge_ = std::make_unique<bridge::EventBridge>(bridge_logger, webview);

        auto manager_ptr = std::shared_ptr<startup::StartupManager>(
            startup_manager_.get(),
            [](startup::StartupManager*){}
        );

        bridge_->connect_startup_events(manager_ptr);

        logger_->info("Bridge initialized and connected to startup events");
    }

    // Explicit template instantiation for saucer::smartview
    template void core::init_bridge(saucer::smartview<>*);

    bool core::start_startup() {
        logger_->info("Starting startup sequence...");
        auto result = startup_manager_->start_async();
        if (!result.ok) {
            logger_->error("Failed to start startup sequence: {}", result.error);
            return false;
        }
        return true;
    }

    void core::abort_startup() {
        logger_->info("Aborting startup sequence...");
        startup_manager_->request_abort(startup::AbortReason::UserRequested);
    }

    bool core::retry_startup() {
        logger_->info("Retrying startup sequence...");
        auto result = startup_manager_->retry_async();
        if (!result.ok) {
            logger_->error("Failed to retry startup sequence: {}", result.error);
            return false;
        }
        return true;
    }

    void core::set_test_mode(int mode) {
        using namespace startup;
        
        std::vector<std::unique_ptr<IStartupStep>> steps;
        
        switch (mode) {
            case 1:  // With failure
                steps.push_back(std::make_unique<CheckJackInstallationStep>());
                steps.push_back(std::make_unique<DiscoverNMOSRegistryStep>());
                steps.push_back(std::make_unique<FailingStep>());
                steps.push_back(std::make_unique<LoadAudioDevicesStep>());
                break;
            case 2:  // With timeout
                steps.push_back(std::make_unique<CheckJackInstallationStep>());
                steps.push_back(std::make_unique<SlowStep>());
                steps.push_back(std::make_unique<LoadAudioDevicesStep>());
                break;
            default:  // Normal
                steps = create_default_steps();
                break;
        }
        
        startup_manager_->clear_steps();
        startup_manager_->set_steps(std::move(steps));
        logger_->info("Set test mode: {}", mode);
    }

    std::string core::get_settings_json() {
        auto snapshot = settings_.snapshot();
        return settings::to_json_string(*snapshot);
    }

    std::string core::get_pending_settings_json() {
        auto pending = settings_.pending_copy();
        return settings::to_json_string(pending);
    }

    std::string core::stage_settings_json(const std::string& json_str) {
        settings::AppSettings new_settings;
        auto parse_result = settings::from_json_string(json_str, new_settings);

        if (!parse_result.ok) {
            nlohmann::json error_json = {
                {"ok", false},
                {"error", "Failed to parse settings JSON: " + parse_result.error}
            };
            return error_json.dump();
        }

        auto stage_result = settings_.stage([&new_settings](settings::AppSettings& s) {
            s = new_settings;
        });

        nlohmann::json result_json = {
            {"ok", stage_result.ok},
            {"error", stage_result.error}
        };
        return result_json.dump();
    }

    std::string core::save_settings_json() {
        auto [result, impact] = settings_.save();

        nlohmann::json result_json = {
            {"ok", result.ok},
            {"error", result.error}
        };

        nlohmann::json impact_json = {
            {"app_restart_required", impact.app_restart_required},
            {"modules_restart_required", impact.modules_restart_required},
            {"restart_sensitive_keys_changed", impact.restart_sensitive_keys_changed}
        };

        nlohmann::json response = {
            {"result", result_json},
            {"save_impact", impact_json}
        };

        return response.dump();
    }

    std::string core::reset_pending_settings_json() {
        auto result = settings_.reset_pending_to_active();

        nlohmann::json result_json = {
            {"ok", result.ok},
            {"error", result.error}
        };
        return result_json.dump();
    }

    bool core::has_pending_settings_changes() {
        return settings_.has_pending_changes();
    }

    void core::log_aknet_start_message() {
        if (logger_) {
            logger_->info("--------------------------------------------------------");
            logger_->info("      ▄▄                  ██  ");
            logger_->info(" ▀▀█▄ ██ ▄█▀ ████▄ ▄█▀█▄ ▀██▀▀");
            logger_->info("▄█▀██ ████   ██ ██ ██▄█▀  ██  ");
            logger_->info("▀█▄██ ██ ▀█▄ ██ ██ ▀█▄▄▄  ██  ");
            logger_->info("");
            logger_->info("aknet - v{} by {}", PROJECT_VERSION, PROJECT_AUTHOR);
            logger_->info("--------------------------------------------------------");
        }
    }
} // namespace aknet
