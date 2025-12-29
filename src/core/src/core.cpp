//
// Created by Nicolas Désilles on 27/12/2025.
//

#include "core.h"
#include <version.h>

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

        // Future: create owned modules here
        // module_a_ = std::make_unique<ModuleA>();

        logger_->info("Initializing Core: Done.");
    }

    // Destructor
    core::~core() {
        logger_->info("Core shutting down...");

        // 1. Future: modules are destroyed automatically (unique_ptr, reverse order)

        // 2. Release our logger before shutting down logging system
        logger_.reset();

        // 3. Shutdown logging infrastructure last
        log::shutdown();
    }

    void core::test_function() {
        logger_->info("Core test function called");
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
