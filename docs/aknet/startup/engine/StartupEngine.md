---
generator: doxide
---


# StartupEngine

**class StartupEngine final**

Synchronous startup sequence execution engine.

Executes registered steps in order, manages timeouts and abort handling, and tracks progress.
Designed to be used by StartupManager, not directly by application code.


!!! warning

    StartupEngine is NOT thread-safe.
    Use StartupManager for thread-safe async execution.

#### Execution Flow

1. Steps are registered via set_steps() or add_step()
2. run() executes steps sequentially
3. For each step:
   - Check abort flag (if set, mark remaining as Aborted, return)
   - Execute step with StepContext
   - Check timeout/abort after execution
   - If critical step failed/timed out/aborted, stop and set can_retry=true
4. If all steps complete, state transitions to Active

#### Critical vs Non-Critical Steps

- **Critical steps**: Failure stops the entire sequence, sets can_retry=true
- **Non-critical steps**: Failure is logged but sequence continues


## Types

| Name | Description |
| ---- | ----------- |
| [RunOptions](StartupEngine/RunOptions.md) | Options for run() and retry() execution. |

## Type Aliases

| Name | Description |
| ---- | ----------- |
| [StepPtr](#StepPtr) | Unique pointer to a startup step. |

## Variables

| Name | Description |
| ---- | ----------- |
| [logger_](#logger_) | Logger for diagnostic output. |
| [settings_](#settings_) | Settings access for steps. |
| [clock_](#clock_) | Clock for timeout checking. |
| [abort_requested_](#abort_requested_) | Abort request flag (atomic for cross-thread visibility). |
| [abort_reason_](#abort_reason_) | Reason for abort request. |
| [steps_](#steps_) | Registered steps. |
| [progress_](#progress_) | Current progress snapshot. |
| [current_progress_callback_](#current_progress_callback_) | Active progress callback. |
| [current_step_started_callback_](#current_step_started_callback_) | Active step started callback. |
| [current_step_completed_callback_](#current_step_completed_callback_) | Active step completed callback. |

## Functions

| Name | Description |
| ---- | ----------- |
| [StartupEngine](#StartupEngine) | Construct a StartupEngine with dependencies. |
| [set_steps](#set_steps) | Destructor. |
| [add_step](#add_step) | Append a step to the existing sequence. |
| [clear_steps](#clear_steps) | Remove all registered steps. |
| [run](#run) | Execute the startup sequence synchronously. |
| [retry](#retry) | Retry the startup sequence after a failure. |
| [request_abort](#request_abort) | Request abort of the running sequence. |
| [reset_abort](#reset_abort) | Clear the abort flag. |
| [progress](#progress) | Get the current progress snapshot. |
| [state](#state) | Get the current application state. |
| [has_steps](#has_steps) | Check if any steps are registered. |
| [step_count](#step_count) | Get the number of registered steps. |
| [can_retry](#can_retry) | Check if retry is allowed. |
| [rebuild_progress_snapshot](#rebuild_progress_snapshot) | Rebuild progress snapshot from current steps. |
| [run_step](#run_step) | Execute a single step and update progress. |
| [transition_to_off_with_error](#transition_to_off_with_error) | Transition to Off state with an error message. |

## Type Alias Details

### StepPtr<a name="StepPtr"></a>

!!! typedef "using StepPtr = std::unique_ptr&lt;IStartupStep&gt;"

    Unique pointer to a startup step.
    

## Variable Details

### abort_reason_<a name="abort_reason_"></a>

!!! variable "std::atomic&lt;AbortReason&gt; abort_reason_"

    Reason for abort request.
    

### abort_requested_<a name="abort_requested_"></a>

!!! variable "std::atomic_bool abort_requested_"

    Abort request flag (atomic for cross-thread visibility).
    

### clock_<a name="clock_"></a>

!!! variable "std::shared_ptr&lt;IClock&gt; clock_"

    Clock for timeout checking.
    

### current_progress_callback_<a name="current_progress_callback_"></a>

!!! variable "std::function&lt;void(const SequenceProgress&amp;)&gt; current_progress_callback_"

    Active progress callback.
    

### current_step_completed_callback_<a name="current_step_completed_callback_"></a>

!!! variable "std::function&lt;void(int, const std::string&amp;, StepStatus)&gt; current_step_completed_callback_"

    Active step completed callback.
    

### current_step_started_callback_<a name="current_step_started_callback_"></a>

!!! variable "std::function&lt;void(int, const std::string&amp;)&gt; current_step_started_callback_"

    Active step started callback.
    

### logger_<a name="logger_"></a>

!!! variable "std::shared_ptr&lt;log::Logger&gt; logger_"

    Logger for diagnostic output.
    

### progress_<a name="progress_"></a>

!!! variable "SequenceProgress progress_"

    Current progress snapshot.
    

### settings_<a name="settings_"></a>

!!! variable "std::shared_ptr&lt;settings::Settings&gt; settings_"

    Settings access for steps.
    

### steps_<a name="steps_"></a>

!!! variable "std::vector&lt;StepPtr&gt; steps_"

    Registered steps.
    

## Function Details

### StartupEngine<a name="StartupEngine"></a>
!!! function "StartupEngine(std::shared_ptr&lt;log::Logger&gt; logger, std::shared_ptr&lt;settings::Settings&gt; settings, std::shared_ptr&lt;IClock&gt; clock = std::make_shared&lt;SteadyClock&gt;())"

    Construct a StartupEngine with dependencies.
    
    
    :material-location-enter: `logger`
    :    Logger for diagnostic output.
        
    :material-location-enter: `settings`
    :    Settings access for steps.
        
    :material-location-enter: `clock`
    :    Clock for timeout checking (defaults to SteadyClock).
    
    #### Exceptions
    
    - Throws `std::invalid_argument` if logger is null.
    - Throws `std::invalid_argument` if settings is null.
    - Throws `std::invalid_argument` if clock is null.
    

### add_step<a name="add_step"></a>
!!! function "Result add_step(StepPtr step)"

    Append a step to the existing sequence.
    
    Validates the step configuration and ensures ID is unique.
    
    
    :material-location-enter: `step`
    :    Step to add (ownership transferred).
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or validation error.
    

### can_retry<a name="can_retry"></a>
!!! function "bool can_retry() const"

    Check if retry is allowed.
    
    
    :material-keyboard-return: **Return**
    :    True if state is Off and can_retry is true.
    

### clear_steps<a name="clear_steps"></a>
!!! function "void clear_steps()"

    Remove all registered steps.
    
    Resets state to Off with empty progress.
    

### has_steps<a name="has_steps"></a>
!!! function "bool has_steps() const"

    Check if any steps are registered.
    
    
    :material-keyboard-return: **Return**
    :    True if at least one step is registered.
    

### progress<a name="progress"></a>
!!! function "const SequenceProgress&amp; progress() const"

    Get the current progress snapshot.
    
    
    :material-keyboard-return: **Return**
    :    Reference to the SequenceProgress.
    

### rebuild_progress_snapshot<a name="rebuild_progress_snapshot"></a>
!!! function "void rebuild_progress_snapshot(AppState state)"

    Rebuild progress snapshot from current steps.
    

### request_abort<a name="request_abort"></a>
!!! function "void request_abort(AbortReason reason = AbortReason::UserRequested)"

    Request abort of the running sequence.
    
    Sets the abort flag which is checked between steps and can be checked by steps via StepContext.
    
    
    :material-location-enter: `reason`
    :    Why the abort is being requested (default: UserRequested).
    

### reset_abort<a name="reset_abort"></a>
!!! function "void reset_abort()"

    Clear the abort flag.
    
    Called automatically before retry().
    Can be called manually if needed.
    

### retry<a name="retry"></a>
!!! function "Result retry(const RunOptions&amp; options = {true, nullptr, nullptr, nullptr})"

    Retry the startup sequence after a failure.
    
    Only succeeds if state is Off and can_retry is true (set after critical failure).
    Resets abort flag and all steps to Pending before re-running.
    
    
    :material-location-enter: `options`
    :    Execution options (callbacks, reset behavior).
    
    
    :material-keyboard-return: **Return**
    :    Result indicating whether retry was initiated.
    

### run<a name="run"></a>
!!! function "const SequenceProgress&amp; run(const RunOptions&amp; options)"

    Execute the startup sequence synchronously.
    
    Runs all registered steps in order.
    Returns when sequence completes, fails, or is aborted.
    
    
    :material-location-enter: `options`
    :    Execution options (callbacks, reset behavior).
    
    
    :material-keyboard-return: **Return**
    :    Reference to the final progress snapshot.
    
    
    !!! note
    
        This method blocks until the sequence completes.
        For async execution, use StartupManager::start_async().
    

### run_step<a name="run_step"></a>
!!! function "bool run_step(std::size_t index)"

    Execute a single step and update progress.
    
    
    :material-keyboard-return: **Return**
    :    True if sequence should continue, false if stopped.
    

### set_steps<a name="set_steps"></a>
!!! function "Result set_steps(std::vector&lt;StepPtr&gt; steps)"

    Destructor.
    Replace all registered steps.
    
    Validates all configurations and ensures unique IDs.
    Resets progress to Off state with all steps Pending.
    
    
    :material-location-enter: `steps`
    :    Vector of steps to register (ownership transferred).
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or validation error.
    

### state<a name="state"></a>
!!! function "AppState state() const"

    Get the current application state.
    
    
    :material-keyboard-return: **Return**
    :    Current AppState (Off, Booting, Active, ShuttingDown).
    

### step_count<a name="step_count"></a>
!!! function "std::size_t step_count() const"

    Get the number of registered steps.
    
    
    :material-keyboard-return: **Return**
    :    Step count.
    

### transition_to_off_with_error<a name="transition_to_off_with_error"></a>
!!! function "void transition_to_off_with_error(std::string error, bool can_retry)"

    Transition to Off state with an error message.
    

