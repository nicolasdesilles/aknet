---
generator: doxide
---


# Event

**enum class Event : std::uint8_t**

Event types fired by StartupManager.


**ProgressChanged**
:   Fired after each step (with full progress snapshot)


**StateChanged**
:   Fired on AppState transitions (old_state, new_state)


**StepStarted**
:   Fired when step begins (index, id)


**StepCompleted**
:   Fired when step finishes (index, id, status)


**SequenceCompleted**
:   Fired when sequence finishes (success, error_msg)


**Error**
:   Fired on critical errors (error_msg)



