//
// Created by Nicolas Désilles on 27/12/2025.
//

#ifndef AKNET_LOGGER_H
#define AKNET_LOGGER_H

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <filesystem>
#include <format>

namespace aknet::log {

    enum class LogLevel { trace, debug, info, warn, error, critical, off };

    // Forward declaration — implementation is hidden in .cpp
    class LoggerImpl;

    // -------------------------------------------------------------------------
    // Logger: the public interface modules use for logging.
    // Wraps spdlog internally but does not expose spdlog types.
    // -------------------------------------------------------------------------
    class Logger {
    public:
        explicit Logger(std::shared_ptr<LoggerImpl> impl);
        ~Logger();

        // Non-copyable, movable
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) noexcept;
        Logger& operator=(Logger&&) noexcept;

        // Logging methods (variadic, type-safe via std::format)
        template <typename... Args>
        void trace(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::trace, std::format(fmt, std::forward<Args>(args)...));
        }
        template <typename... Args>
        void debug(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::debug, std::format(fmt, std::forward<Args>(args)...));
        }
        template <typename... Args>
        void info(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::info, std::format(fmt, std::forward<Args>(args)...));
        }
        template <typename... Args>
        void warn(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::warn, std::format(fmt, std::forward<Args>(args)...));
        }
        template <typename... Args>
        void error(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::error, std::format(fmt, std::forward<Args>(args)...));
        }
        template <typename... Args>
        void critical(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::critical, std::format(fmt, std::forward<Args>(args)...));
        }

        // Set this logger's level
        void set_level(LogLevel lvl);

        // Get this logger's level
        LogLevel get_level();

        // Flush buffered output
        void flush();

    private:
        void log(LogLevel lvl, std::string_view msg);
        std::shared_ptr<LoggerImpl> impl_;
    };

    // -------------------------------------------------------------------------
    // Global logging system functions
    // -------------------------------------------------------------------------

    // Initialize the logging system (call once at app startup)
    void init(std::filesystem::path log_dir = {});

    // Shutdown the logging system (call once at app shutdown)
    void shutdown();

    // Set the global log level for all loggers
    void set_global_log_level(LogLevel lvl);

    // Create or retrieve a named logger (each module calls this)
    std::shared_ptr<Logger> get(const std::string& name);

} // namespace aknet::log

#endif // AKNET_LOGGER_H