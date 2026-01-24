---
generator: doxide
---


# Logger

**class Logger**

Thread-safe logger instance for a named logging context.

Logger provides the public interface that modules use for logging.
Each logger has a name (e.g., "core", "startup") that appears in log output to identify the source of messages.

Internally wraps spdlog but does not expose spdlog types, allowing the implementation to be changed without affecting client code.

#### Thread Safety

Logger instances are thread-safe.
Multiple threads can log to the same Logger instance concurrently without external synchronization.

#### Logging Methods

All logging methods use `std::format` syntax for type-safe formatting:

```cpp
logger->info("Processing channel {}", channel_id);
logger->warn("Buffer at {}% capacity", fill_percentage);
logger->error("Failed to connect: {}", error_message);
```

#### Performance Considerations

- Format string validation happens at compile time
- Actual formatting only occurs if the message will be logged


!!! note

    Loggers are non-copyable but movable. Obtain instances via aknet::log::get() rather than constructing directly.


:material-eye-outline: **See**
:    get()

:material-eye-outline: **See**
:    LogLevel


## Variables

| Name | Description |
| ---- | ----------- |
| [impl_](#impl_) | Pointer to implementation (pImpl pattern). |

## Operators

| Name | Description |
| ---- | ----------- |
| [operator=](#operator_u003d) | Move assignment operator. |

## Functions

| Name | Description |
| ---- | ----------- |
| [Logger](#Logger) | Constructs a Logger wrapping a LoggerImpl. |
| [~Logger](#_u007eLogger) | Destructor. |
| [Logger](#Logger) | Deleted copy constructor. |
| [trace](#trace) | Log a trace-level message. |
| [debug](#debug) | Log a debug-level message. |
| [info](#info) | Log an info-level message. |
| [warn](#warn) | Log a warning-level message. |
| [error](#error) | Log an error-level message. |
| [critical](#critical) | Log a critical-level message. |
| [set_level](#set_level) | Set this logger's minimum log level. |
| [get_level](#get_level) | Get this logger's current log level. |
| [flush](#flush) | Flush buffered log output immediately. |
| [log](#log) | Internal logging implementation. |

## Variable Details

### impl_<a name="impl_"></a>

!!! variable "std::shared_ptr&lt;LoggerImpl&gt; impl_"

    Pointer to implementation (pImpl pattern).
    

## Operator Details

### operator=<a name="operator_u003d"></a>

!!! function "Logger&amp; operator=(Logger&amp;&amp;) noexcept"

    Move assignment operator.
    

## Function Details

### Logger<a name="Logger"></a>
!!! function "explicit Logger(std::shared_ptr&lt;LoggerImpl&gt; impl)"

    Constructs a Logger wrapping a LoggerImpl.
    
    
    :material-location-enter: `impl`
    :    Shared pointer to the implementation. Must not be null.
    
    
    !!! note
    
        This constructor is public for use by get(), but users should obtain Logger instances via get() rather than constructing directly.
    

!!! function "Logger(Logger&amp;&amp;) noexcept"

    Deleted copy constructor. Loggers are non-copyable.
    Deleted copy assignment. Loggers are non-copyable.
    Move constructor.
    

### critical<a name="critical"></a>
!!! function "template &lt;typename... Args&gt; void critical(std::format_string&lt;Args...&gt; fmt, Args&amp;&amp;... args)"

    Log a critical-level message.
    
    Use for severe error conditions that require immediate attention and likely prevent normal operation.
    
    
    :material-code-tags: `Args`
    :    Format argument types (deduced).
        
    :material-location-enter: `fmt`
    :    Format string using std::format syntax.
        
    :material-location-enter: `args`
    :    Arguments to format into the message.
    
    #### Example
    
    ```cpp
    logger->critical("Audio engine crashed: {}", exception.what());
    logger->critical("Out of memory, cannot allocate buffer");
    ```
    

### debug<a name="debug"></a>
!!! function "template &lt;typename... Args&gt; void debug(std::format_string&lt;Args...&gt; fmt, Args&amp;&amp;... args)"

    Log a debug-level message.
    
    Use for diagnostic information useful during development and debugging.
    Should provide context about what the application is doing without being as verbose as trace.
    
    
    :material-code-tags: `Args`
    :    Format argument types (deduced).
        
    :material-location-enter: `fmt`
    :    Format string using std::format syntax.
        
    :material-location-enter: `args`
    :    Arguments to format into the message.
    
    #### Example
    
    ```cpp
    logger->debug("Audio callback received {} frames", frame_count);
    ```
    

### error<a name="error"></a>
!!! function "template &lt;typename... Args&gt; void error(std::format_string&lt;Args...&gt; fmt, Args&amp;&amp;... args)"

    Log an error-level message.
    
    Use for error conditions that may allow the application to continue running but indicate a failure in some operation.
    
    
    :material-code-tags: `Args`
    :    Format argument types (deduced).
        
    :material-location-enter: `fmt`
    :    Format string using std::format syntax.
        
    :material-location-enter: `args`
    :    Arguments to format into the message.
    
    #### Example
    
    ```cpp
    logger->error("Failed to open audio device: {}", error_msg);
    logger->error("Buffer underrun on channel {}", channel_id);
    ```
    

### flush<a name="flush"></a>
!!! function "void flush()"

    Flush buffered log output immediately.
    
    Forces all buffered log messages to be written to their destinations (console and files).
    Useful before application exit or when immediate output is required for debugging.
    
    
    !!! note
    
        The logging system automatically flushes every 2 seconds, so manual flushing is typically only needed in specific situations.
    

### get_level<a name="get_level"></a>
!!! function "LogLevel get_level()"

    Get this logger's current log level.
    
    
    :material-keyboard-return: **Return**
    :    The current minimum log level.
    
    #### Exceptions
    
    - Throws `std::runtime_error` if the logger is not properly initialized.
    
    
    :material-eye-outline: **See**
    :    set_level()
    

### info<a name="info"></a>
!!! function "template &lt;typename... Args&gt; void info(std::format_string&lt;Args...&gt; fmt, Args&amp;&amp;... args)"

    Log an info-level message.
    
    Use for general operational messages that highlight application progress or state changes.
    These should be meaningful in production without being excessive.
    
    
    :material-code-tags: `Args`
    :    Format argument types (deduced).
        
    :material-location-enter: `fmt`
    :    Format string using std::format syntax.
        
    :material-location-enter: `args`
    :    Arguments to format into the message.
    
    #### Example
    
    ```cpp
    logger->info("Audio engine started with {} channels", channel_count);
    logger->info("Connected to PTP master at {}", master_address);
    ```
    

### log<a name="log"></a>
!!! function "void log(LogLevel lvl, std::string_view msg)"

    Internal logging implementation.
    
    
    :material-location-enter: `lvl`
    :    The log level for this message.
        
    :material-location-enter: `msg`
    :    The formatted message to log.
    

### set_level<a name="set_level"></a>
!!! function "void set_level(LogLevel lvl)"

    Set this logger's minimum log level.
    
    Messages below this level will be filtered out.
    This allows different loggers to have different verbosity levels.
    
    
    :material-location-enter: `lvl`
    :    The minimum level to log.
    
    #### Example
    
    ```cpp
    // Only log warnings and above for this module
    logger->set_level(LogLevel::warn);
    ```
    
    
    :material-eye-outline: **See**
    :    get_level()
    
    :material-eye-outline: **See**
    :    set_global_log_level()
    

### trace<a name="trace"></a>
!!! function "template &lt;typename... Args&gt; void trace(std::format_string&lt;Args...&gt; fmt, Args&amp;&amp;... args)"

    Log a trace-level message.
    
    Use for fine-grained diagnostic output such as function entry/exit, loop iterations, or detailed state dumps.
    Typically disabled in production builds.
    
    
    :material-code-tags: `Args`
    :    Format argument types (deduced).
        
    :material-location-enter: `fmt`
    :    Format string using std::format syntax.
        
    :material-location-enter: `args`
    :    Arguments to format into the message.
    
    #### Example
    
    ```cpp
    logger->trace("Entering process_audio(), buffer_size={}", size);
    ```
    

### warn<a name="warn"></a>
!!! function "template &lt;typename... Args&gt; void warn(std::format_string&lt;Args...&gt; fmt, Args&amp;&amp;... args)"

    Log a warning-level message.
    
    Use for potentially harmful situations that don't prevent operation but should be addressed.
    Examples include approaching resource limits, deprecated feature usage, or recoverable errors.
    
    
    :material-code-tags: `Args`
    :    Format argument types (deduced).
        
    :material-location-enter: `fmt`
    :    Format string using std::format syntax.
        
    :material-location-enter: `args`
    :    Arguments to format into the message.
    
    #### Example
    
    ```cpp
    logger->warn("FIFO buffer at {}% capacity", fill_level);
    logger->warn("Clock drift detected: {} ppm", drift_ppm);
    ```
    

### ~Logger<a name="_u007eLogger"></a>
!!! function "~Logger()"

    Destructor.
    
    Releases the reference to the underlying implementation.
    The actual spdlog logger may persist if other references exist.
    

