#include "logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

namespace aknet::log {

namespace {
    // Shared sinks for all loggers
    std::vector<spdlog::sink_ptr> g_sinks;
    bool g_initialized = false;

    fs::path default_log_dir() {
        const char* home = std::getenv("HOME");
        if (!home) return fs::current_path() / "logs";
        return fs::path(home) / "Desktop" / "Logs" / "aknet";
    }

    std::string session_filename() {
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&t, &tm);
        
        std::ostringstream oss;
        oss << "aknet_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".log";
        return oss.str();
    }
}

void init(fs::path log_dir) {
    if (g_initialized) return;

    if (log_dir.empty()) {
        log_dir = default_log_dir();
    }
    fs::create_directories(log_dir);
    
    const auto log_path = log_dir / session_filename();

    try {
        // File sink: rotating, max 5MB, max 3 files
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path.string(), 1024 * 1024 * 5, 3);
        file_sink->set_level(spdlog::level::trace);
        g_sinks.push_back(file_sink);

        // Console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        g_sinks.push_back(console_sink);

        // Flush all loggers every 2 seconds
        spdlog::flush_every(std::chrono::seconds(2));

        g_initialized = true;
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logging initialization failed: " << ex.what() << std::endl;
    }
}

void shutdown() {
    spdlog::shutdown();
    g_sinks.clear();
    g_initialized = false;
}

void set_global_log_level(LogLevel lvl) {
    spdlog::level::level_enum spd_lvl = spdlog::level::info;
    switch (lvl) {
        case LogLevel::trace: spd_lvl = spdlog::level::trace; break;
        case LogLevel::debug: spd_lvl = spdlog::level::debug; break;
        case LogLevel::info: spd_lvl = spdlog::level::info; break;
        case LogLevel::warn: spd_lvl = spdlog::level::warn; break;
        case LogLevel::error: spd_lvl = spdlog::level::err; break;
        case LogLevel::critical: spd_lvl = spdlog::level::critical; break;
        case LogLevel::off: spd_lvl = spdlog::level::off; break;
    }
    spdlog::set_level(spd_lvl);
}

std::shared_ptr<spdlog::logger> get(const std::string& name) {
    // Check if the logger already exists
    auto logger = spdlog::get(name);
    if (logger) return logger;

    // Create a new logger with shared sinks
    if (!g_initialized || g_sinks.empty()) {
        // Fallback: create a console-only logger if not initialized
        std::cerr << "Logging system not initialized or sink list empty, initializing fallback logger..." << std::endl;
        auto fallback = spdlog::stdout_color_mt(name);
        fallback->set_pattern("%Y-%m-%d %H:%M:%S.%e [%10!n] %^[%8l]%$ %v");
        return fallback;
    }

    auto new_logger = std::make_shared<spdlog::logger>(name, g_sinks.begin(), g_sinks.end());
    new_logger->set_level(spdlog::level::trace);
    // Pattern includes logger name: [level] [name] message
    new_logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%10!n] %^[%8l]%$ %v");
    
    spdlog::register_logger(new_logger);
    return new_logger;
}

} // namespace aknet::log