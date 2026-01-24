---
generator: doxide
---


# RestartRule

**struct RestartRule**

Defines the restart behavior required when a specific setting changes.

Restart rules are registered with the Settings instance to track which settings changes have side effects.
When save() is called, the system compares old and new values for each rule's key and reports the impact.

#### Key Format

Keys use dot notation to reference nested fields:

- `"general.log_level"` - the log_level field in the general section
- `"audio.sampling_rate"` - the sampling_rate field in the audio section


## Variables

| Name | Description |
| ---- | ----------- |
| [key](#key) | Dot-notation path to the setting (e.g., "audio.sampling_rate")  |
| [requires_app_restart](#requires_app_restart) | If true, changing this setting requires a full app restart  |
| [module_name_to_restart](#module_name_to_restart) | Module that needs restart (empty if none or app restart required)  |

## Variable Details

### key<a name="key"></a>

!!! variable "std::string key"

    Dot-notation path to the setting (e.g., "audio.sampling_rate")
    

### module_name_to_restart<a name="module_name_to_restart"></a>

!!! variable "std::string module_name_to_restart"

    Module that needs restart (empty if none or app restart required)
    

### requires_app_restart<a name="requires_app_restart"></a>

!!! variable "bool requires_app_restart"

    If true, changing this setting requires a full app restart
    

