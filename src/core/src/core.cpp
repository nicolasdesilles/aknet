//
// Created by Nicolas Désilles on 27/12/2025.
//

#include "core.h"
#include "version.h"
#include <iostream>

namespace aknet {

    std::shared_ptr<spdlog::logger> core::logger = nullptr;

    void core::init() {

        // When we start the core, we start the tools we need before anything else

        // Initialize the logging system
        log::init();
        log::set_global_log_level(log::LogLevel::trace);

        // Then, we register a logger for this module
        logger = log::get("core");
        logger->set_level(spdlog::level::trace);

        logger->info("aknet - v{0} by {1}", PROJECT_VERSION, PROJECT_AUTHOR);

        logger->info("Initializing core module...");

        logger->info("Initializing core module - done.");

    }

    void core::shutdown() {

        logger->info("Core shutdown called");

        // We let go of our logger
        logger.reset();

        // Shutdown the logging system
        log::shutdown();

    }

    void core::test_function() {
        logger->info("Core test function called");
    }
}
