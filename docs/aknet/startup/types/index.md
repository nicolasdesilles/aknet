---
generator: doxide
---


# types

Core data types, enums, and structs for the startup module.


## Types

| Name | Description |
| ---- | ----------- |
| [AbortReason](AbortReason.md) | Reason for aborting the startup sequence. |
| [AppState](AppState.md) | Application lifecycle state. |
| [IClock](IClock.md) | Abstract interface for time access. |
| [Result](Result.md) | Simple result type for operations that can fail. |
| [SequenceProgress](SequenceProgress.md) | Complete snapshot of startup sequence progress. |
| [SteadyClock](SteadyClock.md) | Get the current time point. |
| [StepConfig](StepConfig.md) | Static configuration for a startup step. |
| [StepProgress](StepProgress.md) | Runtime progress information for a single step. |
| [StepStatus](StepStatus.md) | Execution status of a single startup step. |

## Functions

| Name | Description |
| ---- | ----------- |
| [make_initial_sequence_progress](#make_initial_sequence_progress) | Create initial SequenceProgress for a set of steps. |
| [make_initial_step_progress](#make_initial_step_progress) | Create initial StepProgress from a StepConfig. |
| [validate_step_config](#validate_step_config) | Validate a single step configuration. |
| [validate_step_configs_unique](#validate_step_configs_unique) | Validate that all step IDs in a collection are unique. |

## Function Details

### make_initial_sequence_progress<a name="make_initial_sequence_progress"></a>
!!! function "SequenceProgress make_initial_sequence_progress(AppState state, const std::vector&lt;StepConfig&gt;&amp; configs)"

    Create initial SequenceProgress for a set of steps.
    
    
    :material-location-enter: `state`
    :    Initial AppState to set.
        
    :material-location-enter: `configs`
    :    Vector of step configurations.
    
    
    :material-keyboard-return: **Return**
    :    SequenceProgress with all steps in Pending status.
    

### make_initial_step_progress<a name="make_initial_step_progress"></a>
!!! function "StepProgress make_initial_step_progress(const StepConfig&amp; config)"

    Create initial StepProgress from a StepConfig.
    
    
    :material-location-enter: `config`
    :    The step configuration.
    
    
    :material-keyboard-return: **Return**
    :    StepProgress with status=Pending and id/display_name copied from config.
    

### validate_step_config<a name="validate_step_config"></a>
!!! function "Result validate_step_config(const StepConfig&amp; config)"

    Validate a single step configuration.
    
    Checks that id and display_name are non-empty and timeout is non-negative.
    
    
    :material-location-enter: `config`
    :    The step configuration to validate.
    
    
    :material-keyboard-return: **Return**
    :    Result with ok=true if valid, or error message if invalid.
    

### validate_step_configs_unique<a name="validate_step_configs_unique"></a>
!!! function "Result validate_step_configs_unique(const std::vector&lt;StepConfig&gt;&amp; configs)"

    Validate that all step IDs in a collection are unique.
    
    
    :material-location-enter: `configs`
    :    Vector of step configurations to check.
    
    
    :material-keyboard-return: **Return**
    :    Result with ok=true if all unique, or error message naming the duplicate.
    

