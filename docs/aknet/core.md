---
generator: doxide
---


# core

**class core**

Central orchestrator for the aknet application.

The core class owns all modules and manages their lifecycle. It provides
high-level methods for controlling the application and accessing module functionality.

#### Module Ownership

| Module | Lifetime | Access |
|--------|----------|--------|
| Logger | Core construction → destruction | Via log::get() |
| Settings | Core construction → destruction | Via settings() |
| StartupManager | Core construction → destruction | Via startup_manager() |
| EventBridge | init_bridge() → destruction | Internal only |

#### Thread Safety

Core methods should be called from the main thread. The owned modules
have their own thread safety guarantees (e.g., StartupManager is thread-safe).

#### Example

```cpp
aknet::core app;

// Access settings
auto snapshot = app.settings().snapshot();
int sample_rate = snapshot->audio.sampling_rate;

// Control startup
app.start_startup();

// Handle user cancel
app.abort_startup();

// Retry after failure
if (app.startup_manager().can_retry()) {
    app.retry_startup();
}
```


## Functions

| Name | Description |
| ---- | ----------- |
| [core](#core) | Construct and initialize the core. |
| [~core](#_u007ecore) | Destructor. |
| [test_function](#test_function) | Temporary test function for development. |
| [startup_manager](#startup_manager) | Get the startup manager. |
| [startup_manager](#startup_manager) | Get the startup manager (const). |
| [init_bridge](#init_bridge) | Initialize the event bridge with a webview. |
| [start_startup](#start_startup) | Start the startup sequence. |
| [abort_startup](#abort_startup) | Abort the running startup sequence. |
| [retry_startup](#retry_startup) | Retry the startup sequence after a failure. |
| [set_test_mode](#set_test_mode) | Set the startup test mode. |
| [settings](#settings) | Get the settings manager. |
| [settings](#settings) | Get the settings manager (const). |

## Function Details

### abort_startup<a name="abort_startup"></a>
!!! function "void abort_startup()"

    Abort the running startup sequence.
    
    Requests cancellation of the current startup sequence with AbortReason::UserRequested.
    The abort is processed asynchronously.
    
    
    !!! note
    
        Safe to call even if not currently running (no-op).
    

### core<a name="core"></a>
!!! function "explicit core(const core_config&amp; config = {})"

    Construct and initialize the core.
    
    Initializes all subsystems in the correct order:
    logging → settings → startup manager.
    
    
    :material-location-enter: `config`
    :    Configuration options. All fields have sensible defaults.
    
    #### Exceptions
    
    May throw if critical initialization fails (e.g., cannot create log directory).
    
    #### Example
    
    ```cpp
    // Use defaults
    aknet::core app;
    
    // Or with custom config
    aknet::core app({
        .log_level = log::LogLevel::debug
    });
    ```
    

### init_bridge<a name="init_bridge"></a>
!!! function "template&lt;typename WebviewT&gt; void init_bridge(WebviewT&#42; webview)"

    Initialize the event bridge with a webview.
    
    Creates the EventBridge and connects it to the StartupManager's events.
    Must be called after the webview is created but before start_startup().
    
    
    :material-code-tags: `WebviewT`
    :    Type with an `execute(const std::string&)` method.
        
    :material-location-enter: `webview`
    :    Pointer to the webview. Must not be null and must remain valid.
    
    
    !!! warning
     Can only be called once. Subsequent calls log a warning and return.
    
    #### Example
    
    ```cpp
    saucer::smartview webview;
    // Configure webview...
    app.init_bridge(&webview);
    ```
    

### retry_startup<a name="retry_startup"></a>
!!! function "bool retry_startup()"

    Retry the startup sequence after a failure.
    
    Can only be called after a critical step failure (when can_retry is true).
    Resets all steps and runs the sequence again.
    
    
    :material-keyboard-return: **Return**
    :    true if retry was initiated, false if cannot retry.
    
    #### Example
    
    ```cpp
    // After receiving startup:completed with success=false
    if (app.startup_manager().can_retry()) {
        app.retry_startup();
    }
    ```
    

### set_test_mode<a name="set_test_mode"></a>
!!! function "void set_test_mode(int mode)"

    Set the startup test mode.
    
    Configures different step sequences for testing various scenarios.
    Useful for development and testing the UI's error handling.
    
    
    :material-location-enter: `mode`
    :    Test mode:
                    - 0: Normal (default steps)
                    - 1: With failure (includes a FailingStep)
                    - 2: With timeout (includes a SlowStep that times out)
    
    
    !!! note
    
        Clears current steps and replaces with the test sequence.
    
    #### Example
    
    ```cpp
    // Test failure handling in UI
    app.set_test_mode(1);
    app.start_startup();  // Will fail at FailingStep
    ```
    

### settings<a name="settings"></a>
!!! function "settings::Settings&amp; settings()"

    Get the settings manager.
    
    
    :material-keyboard-return: **Return**
    :    Reference to the Settings for reading and modifying settings.
    
    #### Example
    
    ```cpp
    // Read current settings
    auto snapshot = app.settings().snapshot();
    std::cout << "Sample rate: " << snapshot->audio.sampling_rate << "\n";
    
    // Modify settings
    app.settings().modify([](auto& s) {
        s.audio.sampling_rate = 96000;
    });
    auto result = app.settings().save();
    ```
    

!!! function "const settings::Settings&amp; settings() const"

    Get the settings manager (const).
    
    
    :material-keyboard-return: **Return**
    :    Const reference for read-only access.
    

### start_startup<a name="start_startup"></a>
!!! function "bool start_startup()"

    Start the startup sequence.
    
    Begins asynchronous execution of the registered startup steps.
    Progress is reported via the EventBridge to the frontend.
    
    
    :material-keyboard-return: **Return**
    :    true if startup was initiated, false if already running or no steps.
    
    #### Example
    
    ```cpp
    if (!app.start_startup()) {
        // Handle error - maybe already running
    }
    ```
    

### startup_manager<a name="startup_manager"></a>
!!! function "startup::StartupManager&amp; startup_manager()"

    Get the startup manager.
    
    
    :material-keyboard-return: **Return**
    :    Reference to the StartupManager for advanced control.
    
    
    !!! note
    
        Prefer using start_startup(), abort_startup(), retry_startup() for common operations.
    

!!! function "const startup::StartupManager&amp; startup_manager() const"

    Get the startup manager (const).
    
    
    :material-keyboard-return: **Return**
    :    Const reference for read-only access.
    

### test_function<a name="test_function"></a>
!!! function "void test_function()"

    Temporary test function for development.
    
    
    !!! warning
    
        Will be removed before release.
    

### ~core<a name="_u007ecore"></a>
!!! function "~core()"

    Destructor.
    
    Shuts down all modules in reverse initialization order.
    Ensures clean disconnection of bridge and proper resource cleanup.
    

