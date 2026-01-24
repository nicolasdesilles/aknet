---
generator: doxide
---


# log

Logging utility module.

This util provides a centralized, thread-safe logging system for the app.
This util is meant to be owned by the core.

It wraps spdlog internally while exposing a clean, type-safe API that uses C++23 `std::format` for message formatting.


!!! info

    The other modules in the application will not create their own logger.
    Instead, the core will create a logger and pass it to the module on initialisation.

## Design Goals

- **Encapsulation**: spdlog types are not exposed in the public API, allowing the underlying implementation to change without affecting client code.
- **Thread Safety**: All global operations are mutex-protected. Individual logger instances delegate to thread-safe spdlog loggers.
- **Type Safety**: Uses `std::format_string` for compile-time format string validation.
- **Multiple Sinks**: Logs simultaneously to console (with color) and rotating log files.



!!! note

    The logging system must be initialized with init() before creating any loggers.
    Attempting to get a logger before initialization will throw a `std::runtime_error`.


## Types

| Name | Description |
| ---- | ----------- |
| [LogLevel](LogLevel.md) | Log severity levels. |
| [Logger](Logger.md) | Thread-safe logger instance for a named logging context. |

## Functions

| Name | Description |
| ---- | ----------- |
| [get](#get) | Create or retrieve a named logger. |
| [init](#init) | Initialize the logging system. |
| [is_initialized](#is_initialized) | Check if the logging system is initialized. |
| [set_global_log_level](#set_global_log_level) | Set the log level for all loggers globally. |
| [shutdown](#shutdown) | Shutdown the logging system. |
| [string_to_log_level](#string_to_log_level) | Convert a string to a LogLevel enum value. |

## Function Details

### get<a name="get"></a>
!!! function "std::shared_ptr&lt;Logger&gt; get(const std::string&amp; name)"

    Create or retrieve a named logger.
    
    Returns a shared pointer to a Logger with the specified name.
    If a logger with this name already exists, returns the existing instance.
    Otherwise, creates a new logger that writes to the shared console and file sinks.
    
    
    
    :material-location-enter: `name`
    :    The logger name. Appears in log output to identify the source.
                    Must not be empty.
    
    
    :material-keyboard-return: **Return**
    :    Shared pointer to the Logger instance.
    
    #### Exceptions
    
    - Throws `std::runtime_error` if the logging system is not initialized.
    - Throws `std::invalid_argument` if the name is empty.
    
    #### Example
    
    ```cpp
    auto logger = aknet::log::get("AudioEngine");
    logger->info("Engine initialized");
    ```
    
    
    !!! note
    
        Logger instances are cached and reused. Getting the same name multiple times returns the same underlying logger.
    

### init<a name="init"></a>
!!! function "void init(std::filesystem::path log_dir = {})"

    Initialize the logging system.
    
    Must be called once at application startup before any loggers are created.
    Sets up the console and file sinks that all loggers will share.
    
    #### File Logging
    
    Log files are written to the specified directory (or a default location)
    with rotating file support:
    
    - Maximum file size: 5 MB
    - Maximum number of files: 3
    - Filename format: `aknet_YYYYMMDD_HHMMSS.log`
    
    #### Default Log Directory
    
    If no path is specified, logs are written to:
    
    - macOS: `~/Desktop/Logs/aknet/`
    
    
    !!! warning
    
        This path is very much temporary and needs to be changed!!
    
    
    !!! warning
    
        The default log directory is currently macOS-specific.
        Cross-platform support is planned for future releases.
    
    
    :material-location-enter: `log_dir`
    :    Optional path to the directory for log files. If empty, uses the platform default.
    
    
    !!! note
    
        Calling init() multiple times is safe; later calls are no-ops.
    
    #### Example
    
    ```cpp
    // Use default log directory
    aknet::log::init();
    
    // Or specify a custom directory
    aknet::log::init("/var/log/aknet");
    ```
    
    
    :material-eye-outline: **See**
    :    shutdown()
    
    :material-eye-outline: **See**
    :    is_initialized()
    

### is_initialized<a name="is_initialized"></a>
!!! function "bool is_initialized()"

    Check if the logging system is initialized.
    
    
    :material-keyboard-return: **Return**
    :    `true` if init() has been called and shutdown() has not,
            `false` otherwise.
    
    
    :material-eye-outline: **See**
    :    init()
    
    :material-eye-outline: **See**
    :    shutdown()
    

### set_global_log_level<a name="set_global_log_level"></a>
!!! function "void set_global_log_level(LogLevel lvl)"

    Set the log level for all loggers globally.
    
    Affects all existing and future loggers. Individual loggers can still override their level with Logger::set_level().
    
    
    :material-location-enter: `lvl`
    :    The log level to apply globally.
    
    #### Example
    
    ```cpp
    // Enable verbose logging for debugging
    aknet::log::set_global_log_level(LogLevel::trace);
    
    // Reduce noise in production
    aknet::log::set_global_log_level(LogLevel::warn);
    ```
    
    
    :material-eye-outline: **See**
    :    Logger::set_level()
    

### shutdown<a name="shutdown"></a>
!!! function "void shutdown()"

    Shutdown the logging system.
    
    Flushes all pending log messages, releases resources, and clears all cached loggers.
    Should be called once at core shutdown.
    
    After shutdown, is_initialized() returns false and new loggers cannot be created until init() is called again.
    
    
    !!! note
    
        Any existing Logger instances become invalid after shutdown.
    
    
    :material-eye-outline: **See**
    :    init()
    

### string_to_log_level<a name="string_to_log_level"></a>
!!! function "LogLevel string_to_log_level(std::string_view lvl_str)"

    Convert a string to a LogLevel enum value.
    
    Useful for parsing log levels from settings files.
    
    
    :material-location-enter: `lvl_str`
    :    The log level name (case-sensitive).
                       Valid values: "trace", "debug", "info", "warn", "error", "critical", "off".
    
    
    :material-keyboard-return: **Return**
    :    The corresponding LogLevel enum value.
    
    #### Exceptions
    
    - Throws `std::invalid_argument` if the string is not a valid log level name.
    
    #### Example
    
    ```cpp
    auto level = aknet::log::string_to_log_level("debug");
    logger->set_level(level);
    ```
    

