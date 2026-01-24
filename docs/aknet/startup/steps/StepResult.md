---
generator: doxide
---


# StepResult

**struct StepResult**

Result returned by a step's run() method.

Contains the final status of the step and an optional message.
The engine may override the status (e.g., to TimedOut) based on its own checks.


## Variables

| Name | Description |
| ---- | ----------- |
| [status](#status) | Final status of the step  |
| [message](#message) | Optional status message or error description  |

## Variable Details

### message<a name="message"></a>

!!! variable "std::string message"

    Optional status message or error description
    

### status<a name="status"></a>

!!! variable "StepStatus status"

    Final status of the step
    

