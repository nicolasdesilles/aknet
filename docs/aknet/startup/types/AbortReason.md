---
generator: doxide
---


# AbortReason

**enum class AbortReason**

Reason for aborting the startup sequence.

When the startup sequence is aborted, this enum indicates why.
Used for logging, UI display, and determining whether retry is appropriate.


**None**
:   No abort requested (normal operation)


**UserRequested**
:   User clicked "Cancel" or similar


**Timeout**
:   Step exceeded its timeout (future use)


**CriticalFailure**
:   Critical step failed (future use)


**SystemShutdown**
:   Application is shutting down



