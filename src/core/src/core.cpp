//
// Created by Nicolas Désilles on 27/12/2025.
//

#include "core.h"
#include <version.h>
#include <bridge.h>
#include <saucer/smartview.hpp>

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

    void core::process_bridge_queue() {
        if (bridge_) {
            bridge_->process_queue();
        }
    }

    // Explicit template instantiation for saucer::smartview
    template void core::init_bridge(saucer::smartview<>*);

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
