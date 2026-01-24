/**
 * Logging utility module.
 *
 * This util provides a centralized, thread-safe logging system for the app.
 * This util is meant to be owned by the core.
 *
 * It wraps spdlog internally while exposing a clean, type-safe API that uses C++23 `std::format` for message formatting.
 *
 * @info
 * The other modules in the application will not create their own logger.
 * Instead, the core will create a logger and pass it to the module on initialisation.
 *
 * ## Design Goals
 *
 * - **Encapsulation**: spdlog types are not exposed in the public API, allowing the underlying implementation to change without affecting client code.
 * - **Thread Safety**: All global operations are mutex-protected. Individual logger instances delegate to thread-safe spdlog loggers.
 * - **Type Safety**: Uses `std::format_string` for compile-time format string validation.
 * - **Multiple Sinks**: Logs simultaneously to console (with color) and rotating log files.
 *
 *
 * @note
 * The logging system must be initialized with init() before creating any loggers.
 * Attempting to get a logger before initialization will throw a `std::runtime_error`.
 */

#ifndef AKNET_LOGGER_H
#define AKNET_LOGGER_H

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <filesystem>
#include <format>

namespace aknet::log {

    /**
     * Log severity levels.
     *
     * Levels are ordered from most verbose (trace) to most severe (critical), with `off` disabling all logging.
     * When a log level is set, only messages at that level or higher severity are output.
     *
     * | Level    | Use Case                                              |
     * |----------|-------------------------------------------------------|
     * | trace    | Fine-grained debugging, function entry/exit           |
     * | debug    | Diagnostic information for development                |
     * | info     | General operational messages                          |
     * | warn     | Potentially harmful situations                        |
     * | error    | Error events that might allow continued operation     |
     * | critical | Severe errors requiring immediate attention           |
     * | off      | Disable all logging output                            |
     */
    enum class LogLevel {
        trace,    ///< Most verbose: detailed tracing information
        debug,    ///< Debug-level messages for development
        info,     ///< Informational messages about normal operation
        warn,     ///< Warning conditions that should be addressed
        error,    ///< Error conditions that may allow recovery
        critical, ///< Critical failures requiring immediate attention
        off       ///< Disable all logging
    };

    // Forward declaration — implementation is hidden in .cpp
    class LoggerImpl;

    /**
     * Thread-safe logger instance for a named logging context.
     *
     * Logger provides the public interface that modules use for logging.
     * Each logger has a name (e.g., "core", "startup") that appears in log output to identify the source of messages.
     *
     * Internally wraps spdlog but does not expose spdlog types, allowing the implementation to be changed without affecting client code.
     *
     * #### Thread Safety
     *
     * Logger instances are thread-safe.
     * Multiple threads can log to the same Logger instance concurrently without external synchronization.
     *
     * #### Logging Methods
     *
     * All logging methods use `std::format` syntax for type-safe formatting:
     *
     * ```cpp
     * logger->info("Processing channel {}", channel_id);
     * logger->warn("Buffer at {}% capacity", fill_percentage);
     * logger->error("Failed to connect: {}", error_message);
     * ```
     *
     * #### Performance Considerations
     *
     * - Format string validation happens at compile time
     * - Actual formatting only occurs if the message will be logged
     *
     * @note
     * Loggers are non-copyable but movable. Obtain instances via aknet::log::get() rather than constructing directly.
     *
     * @see get()
     * @see LogLevel
     */
    class Logger {
    public:
        /**
         * Constructs a Logger wrapping a LoggerImpl.
         *
         * @param impl Shared pointer to the implementation. Must not be null.
         *
         * @note
         * This constructor is public for use by get(), but users should obtain Logger instances via get() rather than constructing directly.
         */
        explicit Logger(std::shared_ptr<LoggerImpl> impl);

        /**
         * Destructor.
         *
         * Releases the reference to the underlying implementation.
         * The actual spdlog logger may persist if other references exist.
         */
        ~Logger();

        // -- Copy/Move Semantics --

        /** Deleted copy constructor. Loggers are non-copyable. */
        Logger(const Logger&) = delete;

        /** Deleted copy assignment. Loggers are non-copyable. */
        Logger& operator=(const Logger&) = delete;

        /** Move constructor. */
        Logger(Logger&&) noexcept;

        /** Move assignment operator. */
        Logger& operator=(Logger&&) noexcept;

        // -- Logging Methods --

        /**
         * Log a trace-level message.
         *
         * Use for fine-grained diagnostic output such as function entry/exit, loop iterations, or detailed state dumps.
         * Typically disabled in production builds.
         *
         * @tparam Args Format argument types (deduced).
         * @param fmt Format string using std::format syntax.
         * @param args Arguments to format into the message.
         *
         * #### Example
         *
         * ```cpp
         * logger->trace("Entering process_audio(), buffer_size={}", size);
         * ```
         */
        template <typename... Args>
        void trace(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::trace, std::format(fmt, std::forward<Args>(args)...));
        }

        /**
         * Log a debug-level message.
         *
         * Use for diagnostic information useful during development and debugging.
         * Should provide context about what the application is doing without being as verbose as trace.
         *
         * @tparam Args Format argument types (deduced).
         * @param fmt Format string using std::format syntax.
         * @param args Arguments to format into the message.
         *
         * #### Example
         *
         * ```cpp
         * logger->debug("Audio callback received {} frames", frame_count);
         * ```
         */
        template <typename... Args>
        void debug(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::debug, std::format(fmt, std::forward<Args>(args)...));
        }

        /**
         * Log an info-level message.
         *
         * Use for general operational messages that highlight application progress or state changes.
         * These should be meaningful in production without being excessive.
         *
         * @tparam Args Format argument types (deduced).
         * @param fmt Format string using std::format syntax.
         * @param args Arguments to format into the message.
         *
         * #### Example
         *
         * ```cpp
         * logger->info("Audio engine started with {} channels", channel_count);
         * logger->info("Connected to PTP master at {}", master_address);
         * ```
         */
        template <typename... Args>
        void info(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::info, std::format(fmt, std::forward<Args>(args)...));
        }

        /**
         * Log a warning-level message.
         *
         * Use for potentially harmful situations that don't prevent operation but should be addressed.
         * Examples include approaching resource limits, deprecated feature usage, or recoverable errors.
         *
         * @tparam Args Format argument types (deduced).
         * @param fmt Format string using std::format syntax.
         * @param args Arguments to format into the message.
         *
         * #### Example
         *
         * ```cpp
         * logger->warn("FIFO buffer at {}% capacity", fill_level);
         * logger->warn("Clock drift detected: {} ppm", drift_ppm);
         * ```
         */
        template <typename... Args>
        void warn(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::warn, std::format(fmt, std::forward<Args>(args)...));
        }

        /**
         * Log an error-level message.
         *
         * Use for error conditions that may allow the application to continue running but indicate a failure in some operation.
         *
         * @tparam Args Format argument types (deduced).
         * @param fmt Format string using std::format syntax.
         * @param args Arguments to format into the message.
         *
         * #### Example
         *
         * ```cpp
         * logger->error("Failed to open audio device: {}", error_msg);
         * logger->error("Buffer underrun on channel {}", channel_id);
         * ```
         */
        template <typename... Args>
        void error(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::error, std::format(fmt, std::forward<Args>(args)...));
        }

        /**
         * Log a critical-level message.
         *
         * Use for severe error conditions that require immediate attention and likely prevent normal operation.
         *
         * @tparam Args Format argument types (deduced).
         * @param fmt Format string using std::format syntax.
         * @param args Arguments to format into the message.
         *
         * #### Example
         *
         * ```cpp
         * logger->critical("Audio engine crashed: {}", exception.what());
         * logger->critical("Out of memory, cannot allocate buffer");
         * ```
         */
        template <typename... Args>
        void critical(std::format_string<Args...> fmt, Args&&... args) {
            log(LogLevel::critical, std::format(fmt, std::forward<Args>(args)...));
        }

        // -- Level Management --

        /**
         * Set this logger's minimum log level.
         *
         * Messages below this level will be filtered out.
         * This allows different loggers to have different verbosity levels.
         *
         * @param lvl The minimum level to log.
         *
         * #### Example
         *
         * ```cpp
         * // Only log warnings and above for this module
         * logger->set_level(LogLevel::warn);
         * ```
         *
         * @see get_level()
         * @see set_global_log_level()
         */
        void set_level(LogLevel lvl);

        /**
         * Get this logger's current log level.
         *
         * @return The current minimum log level.
         *
         * #### Exceptions
         *
         * - Throws `std::runtime_error` if the logger is not properly initialized.
         *
         * @see set_level()
         */
        LogLevel get_level();

        /**
         * Flush buffered log output immediately.
         *
         * Forces all buffered log messages to be written to their destinations (console and files).
         * Useful before application exit or when immediate output is required for debugging.
         *
         * @note
         * The logging system automatically flushes every 2 seconds, so manual flushing is typically only needed in specific situations.
         */
        void flush();

    private:
        /**
         * Internal logging implementation.
         *
         * @param lvl The log level for this message.
         * @param msg The formatted message to log.
         */
        void log(LogLevel lvl, std::string_view msg);

        /**
         * Pointer to implementation (pImpl pattern).
         */
        std::shared_ptr<LoggerImpl> impl_;
    };

    // =========================================================================
    // Global Logging System Functions
    // =========================================================================

    /**
     * Initialize the logging system.
     *
     * Must be called once at application startup before any loggers are created.
     * Sets up the console and file sinks that all loggers will share.
     *
     * #### File Logging
     *
     * Log files are written to the specified directory (or a default location)
     * with rotating file support:
     *
     * - Maximum file size: 5 MB
     * - Maximum number of files: 3
     * - Filename format: `aknet_YYYYMMDD_HHMMSS.log`
     *
     * #### Default Log Directory
     *
     * If no path is specified, logs are written to:
     *
     * - macOS: `~/Desktop/Logs/aknet/`
     *
     * @warning
     * This path is very much temporary and needs to be changed!!
     *
     * @warning
     * The default log directory is currently macOS-specific.
     * Cross-platform support is planned for future releases.
     *
     * @param log_dir Optional path to the directory for log files. If empty, uses the platform default.
     *
     * @note
     * Calling init() multiple times is safe; later calls are no-ops.
     *
     * #### Example
     *
     * ```cpp
     * // Use default log directory
     * aknet::log::init();
     *
     * // Or specify a custom directory
     * aknet::log::init("/var/log/aknet");
     * ```
     *
     * @see shutdown()
     * @see is_initialized()
     */
    void init(std::filesystem::path log_dir = {});

    /**
     * Shutdown the logging system.
     *
     * Flushes all pending log messages, releases resources, and clears all cached loggers.
     * Should be called once at core shutdown.
     *
     * After shutdown, is_initialized() returns false and new loggers cannot be created until init() is called again.
     *
     * @note
     * Any existing Logger instances become invalid after shutdown.
     *
     * @see init()
     */
    void shutdown();

    /**
     * Set the log level for all loggers globally.
     *
     * Affects all existing and future loggers. Individual loggers can still override their level with Logger::set_level().
     *
     * @param lvl The log level to apply globally.
     *
     * #### Example
     *
     * ```cpp
     * // Enable verbose logging for debugging
     * aknet::log::set_global_log_level(LogLevel::trace);
     *
     * // Reduce noise in production
     * aknet::log::set_global_log_level(LogLevel::warn);
     * ```
     *
     * @see Logger::set_level()
     */
    void set_global_log_level(LogLevel lvl);

    /**
     * Check if the logging system is initialized.
     *
     * @return `true` if init() has been called and shutdown() has not,
     *         `false` otherwise.
     *
     * @see init()
     * @see shutdown()
     */
    bool is_initialized();

    /**
     * Convert a string to a LogLevel enum value.
     *
     * Useful for parsing log levels from settings files.
     *
     * @param lvl_str The log level name (case-sensitive).
     *                Valid values: "trace", "debug", "info", "warn", "error", "critical", "off".
     *
     * @return The corresponding LogLevel enum value.
     *
     * #### Exceptions
     *
     * - Throws `std::invalid_argument` if the string is not a valid log level name.
     *
     * #### Example
     *
     * ```cpp
     * auto level = aknet::log::string_to_log_level("debug");
     * logger->set_level(level);
     * ```
     */
    LogLevel string_to_log_level(std::string_view lvl_str);

    /**
     * Create or retrieve a named logger.
     *
     * Returns a shared pointer to a Logger with the specified name.
     * If a logger with this name already exists, returns the existing instance.
     * Otherwise, creates a new logger that writes to the shared console and file sinks.
     *
     *
     * @param name The logger name. Appears in log output to identify the source.
     *             Must not be empty.
     *
     * @return Shared pointer to the Logger instance.
     *
     * #### Exceptions
     *
     * - Throws `std::runtime_error` if the logging system is not initialized.
     * - Throws `std::invalid_argument` if the name is empty.
     *
     * #### Example
     *
     * ```cpp
     * auto logger = aknet::log::get("AudioEngine");
     * logger->info("Engine initialized");
     * ```
     *
     * @note
     * Logger instances are cached and reused. Getting the same name multiple times returns the same underlying logger.
     */
    std::shared_ptr<Logger> get(const std::string& name);

} // namespace aknet::log

#endif // AKNET_LOGGER_H
