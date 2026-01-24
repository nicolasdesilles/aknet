---
generator: doxide
---


# core_config

**struct core_config**

Configuration for core initialization.

All paths are optional - defaults are used if not specified.


## Variables

| Name | Description |
| ---- | ----------- |
| [log_dir](#log_dir) | Directory for log files (default: platform-specific)  |
| [settings_dir](#settings_dir) | Directory for settings file (default: platform-specific)  |
| [settings_schema_version](#settings_schema_version) | Settings schema version for migrations  |
| [log_level](#log_level) | Initial log level (may be overridden by saved settings)  |

## Variable Details

### log_dir<a name="log_dir"></a>

!!! variable "std::filesystem::path log_dir"

    Directory for log files (default: platform-specific)
    

### log_level<a name="log_level"></a>

!!! variable "log::LogLevel log_level"

    Initial log level (may be overridden by saved settings)
    

### settings_dir<a name="settings_dir"></a>

!!! variable "std::filesystem::path settings_dir"

    Directory for settings file (default: platform-specific)
    

### settings_schema_version<a name="settings_schema_version"></a>

!!! variable "int settings_schema_version"

    Settings schema version for migrations
    

