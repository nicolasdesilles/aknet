//
// Created by Nicolas Désilles on 27/12/2025.
//

#include "logger.h"

#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace aknet::logger {

    static std::shared_ptr<spdlog::logger> g_logger;

    // ------------------------------------------------------------------------------------------------
    // Private
    // ------------------------------------------------------------------------------------------------

    // Default logs directory
    std::filesystem::path logger::default_macos_log_dir() {

        const char* home = std::getenv("HOME");
        if (!home) return std::filesystem::current_path() / "logs";
        return std::filesystem::path(home) / "Desktop" / "Logs" / "aknet";

    }

    // Logs file name
    std::string logger::session_filename() {

        // Getting the current time
        const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&t, &tm);

        // Generating the logs filename
        std::ostringstream oss;
        oss << "aknet" << "_"
            << std::put_time(&tm, "%Y%m%d_%H%M%S")
            << ".log";

        return oss.str();
    }

    // ------------------------------------------------------------------------------------------------
    // Public
    // ------------------------------------------------------------------------------------------------

    void logger::init(std::filesystem::path log_dir, const bool also_console) {
        if (g_logger) return;

        if (log_dir.empty()) {
            log_dir = default_macos_log_dir();
        }

        const auto log_path = log_dir / session_filename();

        std::vector<spdlog::sink_ptr> sinks;

        if (also_console) {
            const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            sinks.push_back(console_sink);
        }

        // Rotate: 10 MB each, keep 5 files.
        const auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path.string(), 10 * 1024 * 1024, 5);
        sinks.push_back(file_sink);

        g_logger = std::make_shared<spdlog::logger>("aknet", sinks.begin(), sinks.end());
        spdlog::set_default_logger(g_logger);

        spdlog::set_level(spdlog::level::debug);

        // Timestamp + level + source + message
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e | [%l] %v");
    }

    void logger::shutdown() {
        spdlog::shutdown();
        g_logger.reset();
    }

    std::unique_ptr<logger> create_logger() {
        return std::make_unique<logger>();
    }
}