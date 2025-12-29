#include "logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace fs = std::filesystem;

namespace aknet::log {

    // -------------------------------------------------------------------------
    // LoggerImpl: the hidden implementation that holds a spdlog::logger
    // -------------------------------------------------------------------------
    class LoggerImpl {
    public:
        explicit LoggerImpl(std::shared_ptr<spdlog::logger> spd) : spd_(std::move(spd)) {}

        void log(LogLevel lvl, std::string_view msg) {
            spd_->log(to_spdlog_level(lvl), "{}", msg);
        }

        void set_level(LogLevel lvl) {
            spd_->set_level(to_spdlog_level(lvl));
        }

        LogLevel get_level() {
            switch (spdlog::level::level_enum spd_lvl = spd_->level()) {
                case spdlog::level::trace: return LogLevel::trace;
                case spdlog::level::debug: return LogLevel::debug;
                case spdlog::level::info: return LogLevel::info;
                case spdlog::level::warn: return LogLevel::warn;
                case spdlog::level::err: return LogLevel::error;
                case spdlog::level::critical: return LogLevel::critical;
                default: return LogLevel::off;
            }

        }

        void flush() {
            spd_->flush();
        }

    private:
        static spdlog::level::level_enum to_spdlog_level(LogLevel lvl) {
            switch (lvl) {
                case LogLevel::trace: return spdlog::level::trace;
                case LogLevel::debug: return spdlog::level::debug;
                case LogLevel::info: return spdlog::level::info;
                case LogLevel::warn: return spdlog::level::warn;
                case LogLevel::error: return spdlog::level::err;
                case LogLevel::critical: return spdlog::level::critical;
                case LogLevel::off: return spdlog::level::off;
            }
            return spdlog::level::info;
        }

        std::shared_ptr<spdlog::logger> spd_;
    };

    // -------------------------------------------------------------------------
    // Global state (protected by mutex)
    // -------------------------------------------------------------------------
    namespace {
        std::mutex g_mutex;
        std::vector<spdlog::sink_ptr> g_sinks;
        std::unordered_map<std::string, std::shared_ptr<Logger>> g_loggers;
        bool g_initialized = false;
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    /*
     * WARNING: hardcoded the default logs dir to "home/Desktop/Logs/aknet in the logger.cpp
     * This will only work on macOS.
     * --> TO UPDATE LATER!
     * To do: have a better default logs location for all plateforms
     */
    fs::path default_log_dir() {
        const char* home = std::getenv("HOME");
        if (!home) return fs::current_path() / "logs" ;
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

    // -------------------------------------------------------------------------
    // Logger implementation
    // -------------------------------------------------------------------------
    Logger::Logger(std::shared_ptr<LoggerImpl> impl) : impl_(std::move(impl)) {}
    Logger::~Logger() = default;
    Logger::Logger(Logger&&) noexcept = default;
    Logger& Logger::operator=(Logger&&) noexcept = default;

    void Logger::log(LogLevel lvl, std::string_view msg) {
        if (impl_) impl_->log(lvl, msg);
    }

    void Logger::set_level(LogLevel lvl) {
        if (impl_) impl_->set_level(lvl);
    }

    LogLevel Logger::get_level() {
        if (impl_) return impl_->get_level();
        throw std::runtime_error("Cannot get logger level because logger not initialized");
    }

    void Logger::flush() {
        if (impl_) impl_->flush();
    }

    // -------------------------------------------------------------------------
    // Global functions
    // -------------------------------------------------------------------------
    void init(fs::path log_dir) {
        std::lock_guard lock(g_mutex);

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
        std::lock_guard lock(g_mutex);

        spdlog::shutdown();
        g_sinks.clear();
        g_loggers.clear();
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

    bool is_initialized() {
        return g_initialized;
    }

    std::shared_ptr<Logger> get(const std::string& name) {
        std::lock_guard lock(g_mutex);

        // Check if we already have a wrapper for this logger
        if (auto it = g_loggers.find(name); it != g_loggers.end()) {
            return it->second;
        }

        // Check if the logger already exists in the spdlog registry, but we don't have it cached
        auto spd_logger = spdlog::get(name);
        if (spd_logger) {
            auto logger = std::make_shared<Logger>(std::make_shared<LoggerImpl>(spd_logger));
            g_loggers[name] = logger;
            return logger;
        }

        // Make sure that the logging system is initialized
        if (!g_initialized || g_sinks.empty()) {
            std::cerr << "Logging system not initialized. Call log::init() first." << std::endl;
            throw std::runtime_error("Logging system not initialized");
        }

        // Create anew spdlog logger with shared sinks
        if (empty(name)) {
            throw std::invalid_argument("Logger name cannot be empty");
        }
        auto new_spd_logger = std::make_shared<spdlog::logger>(name, g_sinks.begin(), g_sinks.end());
        new_spd_logger->set_level(spdlog::level::trace);
        new_spd_logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%10!n] %^[%8l]%$ %v");
        spdlog::register_logger(new_spd_logger);

        auto logger = std::make_shared<Logger>(std::make_shared<LoggerImpl>(new_spd_logger));
        g_loggers[name] = logger;
        return logger;
    }

} // namespace aknet::log