---
generator: doxide
---


# Result

**struct Result**

Simple result type for operations that can fail.

Used throughout the settings system instead of exceptions for expected failure cases like file I/O errors.


## Variables

| Name | Description |
| ---- | ----------- |
| [ok](#ok) | True if the operation succeeded  |
| [error](#error) | Error message if ok is false  |

## Variable Details

### error<a name="error"></a>

!!! variable "std::string error"

    Error message if ok is false
    

### ok<a name="ok"></a>

!!! variable "bool ok"

    True if the operation succeeded
    

