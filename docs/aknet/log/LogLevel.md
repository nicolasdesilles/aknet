---
generator: doxide
---


# LogLevel

**enum class LogLevel**

Log severity levels.

Levels are ordered from most verbose (trace) to most severe (critical), with `off` disabling all logging.
When a log level is set, only messages at that level or higher severity are output.

| Level    | Use Case                                              |
|----------|-------------------------------------------------------|
| trace    | Fine-grained debugging, function entry/exit           |
| debug    | Diagnostic information for development                |
| info     | General operational messages                          |
| warn     | Potentially harmful situations                        |
| error    | Error events that might allow continued operation     |
| critical | Severe errors requiring immediate attention           |
| off      | Disable all logging output                            |


**trace**
:   Most verbose: detailed tracing information


**debug**
:   Debug-level messages for development


**info**
:   Informational messages about normal operation


**warn**
:   Warning conditions that should be addressed


**error**
:   Error conditions that may allow recovery


**critical**
:   Critical failures requiring immediate attention


**off**
:   Disable all logging



