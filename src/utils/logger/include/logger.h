//
// Created by Nicolas Désilles on 27/12/2025.
//

#ifndef AKNET_LOGGER_H
#define AKNET_LOGGER_H

#pragma once

#include <memory>
#include <string>
#include <filesystem>

#include <spdlog/spdlog.h>


namespace aknet::log {

    enum class LogLevel { trace, debug, info, warn, error, critical, off };

    // Initialize the logging system (call once at app startup)
    void init(std::filesystem::path log_dir = {});

    // Shutdown the logging system (call once at app shutdown)
    void shutdown();

    // Set the global log level for all loggers
    void set_global_log_level(LogLevel lvl);

    // Create or retrieve a named logger (each module calls this)
    std::shared_ptr<spdlog::logger> get(const std::string& name);

}



#endif //AKNET_LOGGER_H