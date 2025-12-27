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

namespace aknet::logger {

    // Public API (stable interface)
    class logger {

        public:
            virtual ~logger() = default;

            // Core methods
            static void init(
                std::filesystem::path log_dir = {},
                bool also_console = true);
            static void shutdown();

        private:
            static std::filesystem::path default_macos_log_dir();
            static std::string session_filename();

    };

    // Factory function (dependency injection)
    std::unique_ptr<logger> create_logger();

}

// Convenience macros
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

#endif //AKNET_LOGGER_H