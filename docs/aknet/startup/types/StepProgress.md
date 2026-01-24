---
generator: doxide
---


# StepProgress

**struct StepProgress**

Runtime progress information for a single step.

Tracks the current state and timing of a step during and after execution.
Updated by the engine as the step runs.


## Variables

| Name | Description |
| ---- | ----------- |
| [id](#id) | Step identifier (matches StepConfig::id)  |
| [display_name](#display_name) | Human-readable name (copied from config)  |
| [status](#status) | Current execution status  |
| [message](#message) | Status message or error description  |
| [start_time](#start_time) | When step began  |
| [end_time](#end_time) | When step finished  |

## Variable Details

### display_name<a name="display_name"></a>

!!! variable "std::string display_name"

    Human-readable name (copied from config)
    

### end_time<a name="end_time"></a>

!!! variable "std::optional&lt;std::chrono::time_point&lt;std::chrono::steady_clock&gt;&gt; end_time"

    When step finished
    

### id<a name="id"></a>

!!! variable "std::string id"

    Step identifier (matches StepConfig::id)
    

### message<a name="message"></a>

!!! variable "std::string message"

    Status message or error description
    

### start_time<a name="start_time"></a>

!!! variable "std::optional&lt;std::chrono::time_point&lt;std::chrono::steady_clock&gt;&gt; start_time"

    When step began
    

### status<a name="status"></a>

!!! variable "StepStatus status"

    Current execution status
    

