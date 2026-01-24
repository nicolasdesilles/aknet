---
title: Core Module Guide
---

# Core Module Guide

The core module is the central orchestrator for the aknet application. It owns and manages all other modules, providing a single entry point for application lifecycle management.

## Overview

The core handles:

- **Logging** - Initializes and owns the logging system
- **Settings** - Loads, provides access to, and persists application settings
- **Startup** - Manages the startup sequence via StartupManager
- **UI Bridge** - Connects C++ events to the webview frontend via EventBridge

## Initialization

### Initialization Order

The core initializes modules in a specific order to handle dependencies:

1. **Logging System** - `log::init()` must be called before any logging
2. **Core Logger** - Created for the core module itself
3. **Settings System** - Initialized with config, loads saved settings
4. **Log Level Override** - Applies the log level stored in settings
5. **StartupManager** - Created with logger and settings references
6. **Default Steps** - Registers placeholder startup steps

### Configuration

```cpp
aknet::core_config config{
    .log_dir = "/var/log/aknet",           // Log file directory
    .settings_dir = "~/.config/aknet",      // Settings file directory
    .settings_schema_version = 1,           // For settings migrations
    .log_level = aknet::log::LogLevel::info // Initial log level
};

aknet::core app(config);
```

All configuration fields have sensible defaults, so you can simply use:

```cpp
aknet::core core;  // Uses all defaults
```

### Bridge Initialization

The EventBridge requires a webview, which isn't available at core construction time. Initialize it separately:

```cpp
// After creating the webview
saucer::smartview<> webview;
// ... configure webview ...

// Initialize the bridge
app.init_bridge(&webview);
```

!!! warning "Bridge Timing"
    Call `init_bridge()` before `start_startup()` to ensure the UI receives all events.

## Startup Control

The core provides high-level methods for controlling the startup sequence:

### Starting the Sequence

```cpp
bool success = core.start_startup();
if (!success) {
    // Already running or no steps configured
}
```

### Handling User Cancellation

```cpp
void on_cancel_button_clicked() {
    core.abort_startup();
}
```

### Retrying After Failure

```cpp
void on_retry_button_clicked() {
    if (core.startup_manager().can_retry()) {
        core.retry_startup();
    }
}
```

### Test Modes

For development and testing, you can configure different startup scenarios:

```cpp
// Normal startup
core.set_test_mode(0);

// Include a failing step
core.set_test_mode(1);

// Include a step that times out
core.set_test_mode(2);
```

## Settings Access

Access the settings system through the core:

```cpp
// Read current settings
auto snapshot = core.settings().snapshot();
int sample_rate = snapshot->audio.sampling_rate;
std::string log_level = snapshot->general.log_level;

// Modify settings
core.settings().modify([](settings::AppSettings& s) {
    s.audio.sampling_rate = 96000;
    s.audio.buffer_size = 512;
});

// Save to disk
auto result = core.settings().save();
if (!result.ok) {
    // Handle save error
}
```

## Shutdown

The core handles shutdown automatically in its destructor:

1. Disconnects and destroys the EventBridge
2. Destroys the StartupManager
3. Shuts down the settings system
4. Releases the core logger
5. Shuts down the logging system

```cpp
{
    aknet::core core;
    // ... use core ...
}  // Automatic cleanup here
```

## Complete Example

This example shows the actual pattern used in aknet's `main.cpp`:

```cpp
#include <saucer/smartview.hpp>
#include <saucer/embedded/all.hpp>
#include <core.h>

namespace {
    // Global core instance - created before saucer, destroyed after
    std::unique_ptr<aknet::core> g_core;
}

// Saucer coroutine entry point
coco::stray start(saucer::application* app)
{
    // Create window and webview
    auto window  = saucer::window::create(app).value();
    auto webview = saucer::smartview<>::create({.window = window}).value();

    window->set_title("aknet");

    // Initialize the event bridge (connects C++ events to JS)
    g_core->init_bridge(&webview);

    // Expose startup control functions to JavaScript
    webview.expose("start_startup", []() -> bool { 
        return g_core->start_startup(); 
    });

    webview.expose("abort_startup", []() { 
        g_core->abort_startup(); 
    });

    webview.expose("retry_startup", []() -> bool { 
        return g_core->retry_startup(); 
    });

    webview.expose("set_test_mode", [](int mode) { 
        g_core->set_test_mode(mode); 
    });

    // Serve embedded UI files
    webview.embed(saucer::embedded::all());
    webview.serve("/index.html");
    
    // Show the window
    window->show();

    // Wait for application to finish
    co_await app->finish();
}

int main()
{
    // 1. Initialize core BEFORE saucer application
    const char* home = std::getenv("HOME");
    g_core = std::make_unique<aknet::core>(aknet::core_config{
        .settings_dir = std::filesystem::path(home) / ".config" / "aknet",
        .log_level = aknet::log::LogLevel::info
    });

    // 2. Create and run saucer application
    int result = saucer::application::create({.id = "aknet"})->run(start);

    // 3. Explicit core shutdown AFTER saucer exits
    g_core.reset();

    return result;
}
```

### Key Points

1. **Core Lifetime**: The core is created before saucer and destroyed after - this ensures logging and settings are available throughout the application lifecycle.

2. **Global Instance**: Using a global `unique_ptr` allows the core to be accessed from saucer's exposed functions.

3. **Bridge Initialization**: `init_bridge()` is called after the webview is created but before serving the UI.

4. **Exposed Functions**: Startup control functions are exposed to JavaScript, allowing the React UI to control the startup sequence.

5. **Explicit Shutdown**: `g_core.reset()` ensures clean shutdown with proper logging before `main()` exits.

## Thread Safety

| Operation | Thread Safety |
|-----------|---------------|
| `start_startup()` | Main thread only |
| `abort_startup()` | Thread-safe |
| `retry_startup()` | Main thread only |
| `settings().snapshot()` | Thread-safe |
| `settings().modify()` | Thread-safe |
| `startup_manager()` | Returns thread-safe object |

## API Reference

See the auto-generated API documentation:

- [core](../aknet/core.md) - Main class
- [core_config](../aknet/core_config.md) - Configuration struct
