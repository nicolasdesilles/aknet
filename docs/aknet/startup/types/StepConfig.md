---
generator: doxide
---


# StepConfig

**struct StepConfig**

Static configuration for a startup step.

Defines the immutable properties of a step that are known at registration time.
These values do not change during execution.


## Variables

| Name | Description |
| ---- | ----------- |
| [id](#id) | Unique step identifier  |
| [display_name](#display_name) | Human-readable name for UI  |
| [timeout](#timeout) | Max execution time (0 = unlimited)  |
| [critical](#critical) | If true, failure stops sequence  |
| [can_skip](#can_skip) | If true, step can be skipped  |

## Variable Details

### can_skip<a name="can_skip"></a>

!!! variable "bool can_skip"

    If true, step can be skipped
    

### critical<a name="critical"></a>

!!! variable "bool critical"

    If true, failure stops sequence
    

### display_name<a name="display_name"></a>

!!! variable "std::string display_name"

    Human-readable name for UI
    

### id<a name="id"></a>

!!! variable "std::string id"

    Unique step identifier
    

### timeout<a name="timeout"></a>

!!! variable "std::chrono::seconds timeout"

    Max execution time (0 = unlimited)
    

