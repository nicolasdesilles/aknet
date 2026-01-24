---
generator: doxide
---


# FakeStep

**class FakeStep : public IStartupStep**

Base class for simulated startup steps.

Provides a common implementation that simulates work with a configurable delay.
Checks for abort and timeout during the simulated work period.


!!! warning

    These are placeholder implementations!
    Replace with real implementations before production use.

#### Behavior

1. Logs step start
2. Sleeps in 50ms increments, checking abort/timeout each iteration
3. Returns the configured outcome when delay completes


## Variables

| Name | Description |
| ---- | ----------- |
| [config_](#config_) | Step configuration. |
| [delay_](#delay_) | Simulated execution time. |
| [outcome_](#outcome_) | Status to return after delay. |
| [outcome_message_](#outcome_message_) | Message to return with the outcome. |

## Functions

| Name | Description |
| ---- | ----------- |
| [FakeStep](#FakeStep) | Construct a FakeStep with configuration and behavior. |

## Variable Details

### config_<a name="config_"></a>

!!! variable "StepConfig config_"

    Step configuration.
    

### delay_<a name="delay_"></a>

!!! variable "std::chrono::milliseconds delay_"

    Simulated execution time.
    

### outcome_<a name="outcome_"></a>

!!! variable "StepStatus outcome_"

    Status to return after delay.
    

### outcome_message_<a name="outcome_message_"></a>

!!! variable "std::string outcome_message_"

    Message to return with the outcome.
    

## Function Details

### FakeStep<a name="FakeStep"></a>
!!! function "FakeStep(StepConfig config, std::chrono::milliseconds delay, StepStatus outcome = StepStatus::Success, std::string outcome_message = &quot;&quot;)"

    Construct a FakeStep with configuration and behavior.
    
    
    :material-location-enter: `config`
    :    Step configuration.
        
    :material-location-enter: `delay`
    :    Simulated execution time.
        
    :material-location-enter: `outcome`
    :    Status to return (default: Success).
        
    :material-location-enter: `outcome_message`
    :    Message to return (default: empty).
    

