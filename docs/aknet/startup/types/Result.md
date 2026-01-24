---
generator: doxide
---


# Result

**struct Result**

Simple result type for operations that can fail.

Used throughout the startup module for error handling without exceptions.


## Variables

| Name | Description |
| ---- | ----------- |
| [ok](#ok) | True if operation succeeded  |
| [error](#error) | Error message if ok is false  |

## Variable Details

### error<a name="error"></a>

!!! variable "std::string error"

    Error message if ok is false
    

### ok<a name="ok"></a>

!!! variable "bool ok"

    True if operation succeeded
    

