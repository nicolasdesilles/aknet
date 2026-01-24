---
generator: doxide
---


# EventBridge

**class EventBridge**

Bridge for forwarding C++ events to the webview as JavaScript CustomEvents.

EventBridge connects to event sources (like StartupManager) and forwards their events
to the embedded webview by executing JavaScript that dispatches DOM CustomEvents.

#### Supported Events

When connected to a StartupManager, the following events are forwarded:

| C++ Event | JS Event Name | Payload |
|-----------|---------------|---------|
| ProgressChanged | `startup:progress` | Full SequenceProgress object |
| StateChanged | `startup:state` | `{old_state, new_state}` |
| StepStarted | `startup:step_started` | `{index, id}` |
| StepCompleted | `startup:step_completed` | `{index, id, status}` |
| SequenceCompleted | `startup:completed` | `{success, error}` |
| Error | `startup:error` | `{message}` |

#### Thread Safety

EventBridge is NOT thread-safe. It should be created and used from the main thread.
However, it safely handles events fired from worker threads (like StartupManager's worker).

#### Lifecycle

1. Construct with a logger and webview pointer
2. Call connect_startup_events() to start forwarding
3. Events are automatically forwarded until disconnect() or destruction
4. Destructor automatically disconnects all listeners

#### Example

```cpp
// In Core initialization
auto bridge = std::make_unique<EventBridge>(logger, webview);
bridge->connect_startup_events(startup_manager);

// Events are now forwarded to the webview
// Frontend can listen: window.addEventListener('startup:progress', ...)
```


!!! note
 The webview pointer must remain valid for the lifetime of the EventBridge.


## Functions

| Name | Description |
| ---- | ----------- |
| [EventBridge](#EventBridge) | Construct an EventBridge with a webview. |
| [~EventBridge](#_u007eEventBridge) | Destructor. |
| [connect_startup_events](#connect_startup_events) | Connect to StartupManager events. |
| [disconnect](#disconnect) | Disconnect from all connected event sources. |

## Function Details

### EventBridge<a name="EventBridge"></a>
!!! function "template&lt;typename WebviewT&gt; EventBridge(std::shared_ptr&lt;log::Logger&gt; logger, WebviewT&#42; webview)"

    Construct an EventBridge with a webview.
    
    The constructor is templated to support both the real saucer::webview
    and mock implementations for testing.
    
    
    :material-code-tags: `WebviewT`
    :    Type with an `execute(const std::string&)` method.
        
    :material-location-enter: `logger`
    :    Logger for diagnostic output. Must not be null.
        
    :material-location-enter: `webview`
    :    Pointer to the webview. Must not be null.
    
    #### Exceptions
    
    - Throws `std::invalid_argument` if logger is null.
    - Throws `std::invalid_argument` if webview is null.
    
    #### Example
    
    ```cpp
    auto logger = log::get("bridge");
    auto bridge = std::make_unique<EventBridge>(logger, webview_ptr);
    ```
    

### connect_startup_events<a name="connect_startup_events"></a>
!!! function "void connect_startup_events(std::shared_ptr&lt;startup::StartupManager&gt; manager)"

    Connect to StartupManager events.
    
    Subscribes to all StartupManager events and forwards them to the webview
    as JavaScript CustomEvents.
    
    If already connected to a manager, disconnects first before connecting to the new one.
    
    
    :material-location-enter: `manager`
    :    The StartupManager to connect to. Must not be null.
    
    #### Exceptions
    
    - Throws `std::invalid_argument` if manager is null.
    
    #### Events Forwarded
    
    - `startup:progress` - Full progress snapshot after each step
    - `startup:state` - State transitions (Off → Booting → Active)
    - `startup:step_started` - When a step begins execution
    - `startup:step_completed` - When a step finishes (with status)
    - `startup:completed` - When the entire sequence finishes
    - `startup:error` - Critical errors
    
    #### Example
    
    ```cpp
    bridge->connect_startup_events(startup_manager_ptr);
    
    // In React:
    // useEffect(() => {
    //     window.addEventListener('startup:progress', handleProgress);
    //     return () => window.removeEventListener('startup:progress', handleProgress);
    // }, []);
    ```
    

### disconnect<a name="disconnect"></a>
!!! function "void disconnect()"

    Disconnect from all connected event sources.
    
    Removes all event listeners that were added by connect_*_events() methods.
    Safe to call multiple times or when not connected.
    
    
    !!! note
    
        Called automatically by the destructor.
    

### ~EventBridge<a name="_u007eEventBridge"></a>
!!! function "~EventBridge()"

    Destructor.
    
    Automatically calls disconnect() to remove all event listeners.
    

