---
generator: doxide
---


# SequenceProgress

**struct SequenceProgress**

Complete snapshot of startup sequence progress.

Contains the full state of the startup process at a point in time.
Used for UI updates and state queries.
Designed to be copied and passed around safely (immutable snapshot pattern).


## Variables

| Name | Description |
| ---- | ----------- |
| [state](#state) | Current application state  |
| [current_step_index](#current_step_index) | Index of running step (-1 if none)  |
| [steps](#steps) | Progress for all registered steps  |
| [last_error](#last_error) | Error message if sequence failed  |
| [can_retry](#can_retry) | True if retry is allowed after failure  |
| [abort_reason](#abort_reason) | Why sequence was aborted (if applicable)  |

## Variable Details

### abort_reason<a name="abort_reason"></a>

!!! variable "AbortReason abort_reason"

    Why sequence was aborted (if applicable)
    

### can_retry<a name="can_retry"></a>

!!! variable "bool can_retry"

    True if retry is allowed after failure
    

### current_step_index<a name="current_step_index"></a>

!!! variable "int current_step_index"

    Index of running step (-1 if none)
    

### last_error<a name="last_error"></a>

!!! variable "std::optional&lt;std::string&gt; last_error"

    Error message if sequence failed
    

### state<a name="state"></a>

!!! variable "AppState state"

    Current application state
    

### steps<a name="steps"></a>

!!! variable "std::vector&lt;StepProgress&gt; steps"

    Progress for all registered steps
    

