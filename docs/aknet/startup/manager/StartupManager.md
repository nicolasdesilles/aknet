---
generator: doxide
---


# StartupManager

**class StartupManager final**

Thread-safe asynchronous startup sequence manager.

Provides async execution of startup steps with thread-safe state access and an event system for progress notifications.
This is the primary interface for application code to interact with the startup system.


!!! info

    StartupManager is designed to be owned by the core module.
    It spawns a background worker thread that executes the startup sequence.
    Events are fired from the worker thread - subscribers must handle thread safety.

#### Thread Safety

- All public methods are thread-safe
- Progress queries return copies, not references
- Step modification is blocked while sequence is running
- Events fire from the worker thread

#### Event System

Subscribe to events using the events() accessor:

```cpp
manager->events().get<StartupManager::Event::ProgressChanged>()
    .on([](const SequenceProgress& p) {
        // Handle progress update (called from worker thread!)
    });
```

#### Lifecycle

1. Construct with logger and settings
2. Register steps with set_steps() or add_step()
3. Subscribe to events
4. Call start_async() to begin execution
5. Handle events (ProgressChanged, SequenceCompleted, etc.)
6. On failure with can_retry=true, call retry_async()
7. Destructor waits for completion and cleans up


## Types

| Name | Description |
| ---- | ----------- |
| [Event](StartupManager/Event.md) | Event types fired by StartupManager. |

## Type Aliases

| Name | Description |
| ---- | ----------- |
| [StepPtr](#StepPtr) | Unique pointer to a startup step. |
| [Events](#Events) | Event manager type. |

## Variables

| Name | Description |
| ---- | ----------- |
| [logger_](#logger_) | Logger for diagnostic output. |
| [engine_](#engine_) | Underlying synchronous engine. |
| [worker_thread_](#worker_thread_) | Background worker thread. |
| [thread_ready_](#thread_ready_) | True when worker thread is ready to receive signals. |
| [should_run_](#should_run_) | Signal to worker thread to start execution. |
| [should_stop_](#should_stop_) | Signal to worker thread to stop. |
| [is_running_](#is_running_) | True while sequence is executing. |
| [events_](#events_) | Event manager. |
| [mutex_](#mutex_) | Mutex protecting engine and cached_progress_. |
| [cv_](#cv_) | Condition variable for thread signaling. |
| [cached_progress_](#cached_progress_) | Cached progress snapshot for thread-safe reads. |

## Functions

| Name | Description |
| ---- | ----------- |
| [StartupManager](#StartupManager) | Construct a StartupManager. |
| [~StartupManager](#_u007eStartupManager) | Destructor. |
| [set_steps](#set_steps) | Replace all registered steps. |
| [add_step](#add_step) | Append a step to the existing sequence. |
| [clear_steps](#clear_steps) | Remove all registered steps. |
| [start_async](#start_async) | Start the startup sequence asynchronously. |
| [request_abort](#request_abort) | Request abort of the running sequence. |
| [retry_async](#retry_async) | Retry the startup sequence asynchronously. |
| [get_progress](#get_progress) | Get a copy of the current progress. |
| [get_state](#get_state) | Get the current application state. |
| [is_running](#is_running) | Check if the sequence is currently running. |
| [can_retry](#can_retry) | Check if retry is allowed. |
| [events](#events) | Access the event manager for subscribing to events. |
| [worker_thread_fn](#worker_thread_fn) | Worker thread function. |

## Type Alias Details

### Events<a name="Events"></a>

!!! typedef "using Events = ereignis::manager&lt; ereignis::event&lt;Event::ProgressChanged, void(const SequenceProgress&amp;)&gt;, ereignis::event&lt;Event::StateChanged, void(AppState, AppState)&gt;, ereignis::event&lt;Event::StepStarted, void(int, const std::string&amp;)&gt;, ereignis::event&lt;Event::StepCompleted, void(int, const std::string&amp;, StepStatus)&gt;, ereignis::event&lt;Event::SequenceCompleted, void(bool, const std::string&amp;)&gt;, ereignis::event&lt;Event::Error, void(const std::string&amp;)&gt; &gt;"

    Event manager type.
    
    All events are fired from the worker thread.
    Subscribers must handle thread safety in their handlers.
    

### StepPtr<a name="StepPtr"></a>

!!! typedef "using StepPtr = std::unique_ptr&lt;IStartupStep&gt;"

    Unique pointer to a startup step.
    

## Variable Details

### cached_progress_<a name="cached_progress_"></a>

!!! variable "SequenceProgress cached_progress_"

    Cached progress snapshot for thread-safe reads.
    

### cv_<a name="cv_"></a>

!!! variable "std::condition_variable cv_"

    Condition variable for thread signaling.
    

### engine_<a name="engine_"></a>

!!! variable "std::shared_ptr&lt;StartupEngine&gt; engine_"

    Underlying synchronous engine.
    

### events_<a name="events_"></a>

!!! variable "Events events_"

    Event manager.
    

### is_running_<a name="is_running_"></a>

!!! variable "std::atomic&lt;bool&gt; is_running_"

    True while sequence is executing.
    

### logger_<a name="logger_"></a>

!!! variable "std::shared_ptr&lt;log::Logger&gt; logger_"

    Logger for diagnostic output.
    

### mutex_<a name="mutex_"></a>

!!! variable "mutable std::mutex mutex_"

    Mutex protecting engine and cached_progress_.
    

### should_run_<a name="should_run_"></a>

!!! variable "std::atomic&lt;bool&gt; should_run_"

    Signal to worker thread to start execution.
    

### should_stop_<a name="should_stop_"></a>

!!! variable "std::atomic&lt;bool&gt; should_stop_"

    Signal to worker thread to stop.
    

### thread_ready_<a name="thread_ready_"></a>

!!! variable "std::atomic&lt;bool&gt; thread_ready_"

    True when worker thread is ready to receive signals.
    

### worker_thread_<a name="worker_thread_"></a>

!!! variable "std::thread worker_thread_"

    Background worker thread.
    

## Function Details

### StartupManager<a name="StartupManager"></a>
!!! function "StartupManager( std::shared_ptr&lt;log::Logger&gt; logger, const std::shared_ptr&lt;settings::Settings&gt; &amp;settings, std::shared_ptr&lt;IClock&gt; = std::make_shared&lt;SteadyClock&gt;())"

    Construct a StartupManager.
    
    Spawns a background worker thread that waits for start signals.
    
    
    :material-location-enter: `logger`
    :    Logger for diagnostic output.
        
    :material-location-enter: `settings`
    :    Settings access for steps.
        
    :material-location-enter: `clock`
    :    Clock for timeout checking (defaults to SteadyClock).
    

### add_step<a name="add_step"></a>
!!! function "Result add_step(StepPtr step)"

    Append a step to the existing sequence.
    
    Cannot be called while sequence is running.
    
    
    :material-location-enter: `step`
    :    Step to add (ownership transferred).
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or error (e.g., if running).
    

### can_retry<a name="can_retry"></a>
!!! function "bool can_retry() const"

    Check if retry is allowed.
    
    
    :material-keyboard-return: **Return**
    :    True if last run failed with can_retry=true and not currently running.
    

### clear_steps<a name="clear_steps"></a>
!!! function "void clear_steps()"

    Remove all registered steps.
    
    No-op if sequence is running.
    

### events<a name="events"></a>
!!! function "Events&amp; events()"

    Access the event manager for subscribing to events.
    
    
    :material-keyboard-return: **Return**
    :    Reference to the Events manager.
    
    #### Example
    
    ```cpp
    // Subscribe to progress changes
    manager->events().get<Event::ProgressChanged>()
        .on([](const SequenceProgress& progress) {
            auto json = serialize_progress(progress);
            send_to_ui(json);
        });
    
    // Subscribe to completion
    manager->events().get<Event::SequenceCompleted>()
        .on([](bool success, const std::string& error) {
            if (success) {
                transition_to_main_ui();
            } else {
                show_error_dialog(error);
            }
        });
    ```
    

### get_progress<a name="get_progress"></a>
!!! function "SequenceProgress get_progress() const"

    Get a copy of the current progress.
    
    
    :material-keyboard-return: **Return**
    :    Copy of SequenceProgress (safe to use from any thread).
    

### get_state<a name="get_state"></a>
!!! function "AppState get_state() const"

    Get the current application state.
    
    
    :material-keyboard-return: **Return**
    :    Current AppState.
    

### is_running<a name="is_running"></a>
!!! function "bool is_running() const"

    Check if the sequence is currently running.
    
    
    :material-keyboard-return: **Return**
    :    True if sequence is executing.
    

### request_abort<a name="request_abort"></a>
!!! function "void request_abort(AbortReason reason = AbortReason::UserRequested)"

    Request abort of the running sequence.
    
    Thread-safe; can be called from any thread.
    The abort is processed between steps or when the current step checks for abort.
    
    
    :material-location-enter: `reason`
    :    Why the abort is being requested (default: UserRequested).
    

### retry_async<a name="retry_async"></a>
!!! function "Result retry_async()"

    Retry the startup sequence asynchronously.
    
    Only succeeds if not running and can_retry is true.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating whether retry was initiated.
    

### set_steps<a name="set_steps"></a>
!!! function "Result set_steps(std::vector&lt;StepPtr&gt; steps)"

    Replace all registered steps.
    
    Cannot be called while sequence is running.
    
    
    :material-location-enter: `steps`
    :    Vector of steps to register (ownership transferred).
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or error (e.g., if running).
    

### start_async<a name="start_async"></a>
!!! function "Result start_async()"

    Start the startup sequence asynchronously.
    
    Returns immediately; execution happens on the worker thread.
    Subscribe to events to receive progress updates.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating whether start was initiated.
    
    #### Errors
    
    - Returns error if already running
    - Returns error if no steps configured
    

### worker_thread_fn<a name="worker_thread_fn"></a>
!!! function "void worker_thread_fn()"

    Worker thread function.
    

### ~StartupManager<a name="_u007eStartupManager"></a>
!!! function "~StartupManager()"

    Destructor.
    
    Signals the worker thread to stop, aborts any running sequence (with SystemShutdown reason), and waits for thread completion.
    

