---
generator: doxide
---


# RunOptions

**struct RunOptions**

Options for run() and retry() execution.


## Variables

| Name | Description |
| ---- | ----------- |
| [reset_progress_before_run](#reset_progress_before_run) | If true, reset all steps to Pending before running. |
| [progress_callback](#progress_callback) | Called after each step completes with the full progress snapshot. |
| [step_started_callback](#step_started_callback) | Called when a step begins execution. |
| [step_completed_callback](#step_completed_callback) | Called when a step finishes. |

## Variable Details

### progress_callback<a name="progress_callback"></a>

!!! variable "std::function&lt;void(const SequenceProgress&amp;)&gt; progress_callback"

    Called after each step completes with the full progress snapshot.
    

### reset_progress_before_run<a name="reset_progress_before_run"></a>

!!! variable "bool reset_progress_before_run"

    If true, reset all steps to Pending before running.
    Set to false to resume from current state.
    

### step_completed_callback<a name="step_completed_callback"></a>

!!! variable "std::function&lt;void(int, const std::string&amp;, StepStatus)&gt; step_completed_callback"

    Called when a step finishes.
    
    
    :material-location-enter: `index`
    :    Step index (0-based).
        
    :material-location-enter: `id`
    :    Step identifier.
        
    :material-location-enter: `status`
    :    Final status of the step.
    

### step_started_callback<a name="step_started_callback"></a>

!!! variable "std::function&lt;void(int, const std::string&amp;)&gt; step_started_callback"

    Called when a step begins execution.
    
    
    :material-location-enter: `index`
    :    Step index (0-based).
        
    :material-location-enter: `id`
    :    Step identifier.
    

