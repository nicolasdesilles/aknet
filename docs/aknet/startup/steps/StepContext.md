---
generator: doxide
---


# StepContext

**struct StepContext**

Execution context provided to steps during run().

Contains everything a step needs to execute: abort checking, timeout checking, logging, and settings access.
Created by the engine before calling step.run().

#### Thread Safety

The abort_flag is atomic and can be checked safely.
Logger is thread-safe.
Settings should only be read, not modified, during step execution.

#### Example

```cpp
StepResult run(StepContext& ctx) override {
    for (int i = 0; i < 100; ++i) {
        // Check for abort request periodically
        if (ctx.abort_requested()) {
            return {StepStatus::Aborted, "User cancelled"};
        }

        // Check for timeout
        if (ctx.is_expired()) {
            return {StepStatus::TimedOut, "Operation took too long"};
        }

        ctx.logger->debug("Processing item {}", i);
        do_work();
    }
    return {StepStatus::Success, "Completed"};
}
```


## Variables

| Name | Description |
| ---- | ----------- |
| [abort_flag](#abort_flag) | Pointer to the engine's abort flag. |
| [clock](#clock) | Clock for time-based operations. |
| [deadline](#deadline) | Deadline for this step's execution. |
| [logger](#logger) | Logger for diagnostic output within the step. |
| [settings](#settings) | Settings access (read-only during step execution). |

## Functions

| Name | Description |
| ---- | ----------- |
| [abort_requested](#abort_requested) | Check if abort has been requested. |
| [is_expired](#is_expired) | Check if the step's deadline has passed. |
| [check_abort_point](#check_abort_point) | Throw an exception if abort was requested. |

## Variable Details

### abort_flag<a name="abort_flag"></a>

!!! variable "std::atomic_bool&#42; abort_flag"

    Pointer to the engine's abort flag.
    Check via abort_requested() helper method.
    

### clock<a name="clock"></a>

!!! variable "const IClock&#42; clock"

    Clock for time-based operations.
    Used internally for deadline checking.
    

### deadline<a name="deadline"></a>

!!! variable "std::chrono::steady_clock::time_point deadline"

    Deadline for this step's execution.
    If clock->now() >= deadline, the step has timed out.
    

### logger<a name="logger"></a>

!!! variable "std::shared_ptr&lt;log::Logger&gt; logger"

    Logger for diagnostic output within the step.
    

### settings<a name="settings"></a>

!!! variable "settings::Settings&#42; settings"

    Settings access (read-only during step execution).
    

## Function Details

### abort_requested<a name="abort_requested"></a>
!!! function "bool abort_requested() const"

    Check if abort has been requested.
    
    Call this periodically in long-running operations to allow responsive cancellation.
    
    
    :material-keyboard-return: **Return**
    :    True if abort was requested.
    

### check_abort_point<a name="check_abort_point"></a>
!!! function "void check_abort_point() const"

    Throw an exception if abort was requested.
    
    Alternative to checking abort_requested() - useful for exception-based abort handling.
    
    #### Exceptions
    
    - Throws `std::runtime_error` with message "Step aborted" if abort was requested.
    
    #### Example
    
    ```cpp
    try {
        for (int i = 0; i < 100; ++i) {
            ctx.check_abort_point();
            do_work();
        }
    } catch (const std::runtime_error& e) {
        return {StepStatus::Aborted, e.what()};
    }
    ```
    

### is_expired<a name="is_expired"></a>
!!! function "bool is_expired() const"

    Check if the step's deadline has passed.
    
    
    :material-keyboard-return: **Return**
    :    True if current time >= deadline.
    

