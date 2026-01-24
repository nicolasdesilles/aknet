---
generator: doxide
---


# StepStatus

**enum class StepStatus**

Execution status of a single startup step.

Tracks the lifecycle of an individual step from pending through completion.


**Pending**
:   Step is yet to be started


**Running**
:   Step is in progress


**Success**
:   Step finished successfully


**Failed**
:   Step finished with failure


**TimedOut**
:   Step exceeded its timeout value


**Skipped**
:   Step was skipped


**Aborted**
:   Step was aborted (startup process stopped)



