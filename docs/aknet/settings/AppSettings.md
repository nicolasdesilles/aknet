---
generator: doxide
---


# AppSettings

**struct AppSettings**

Root application settings structure.

Contains all application settings organized into logical groups.
This is the main structure that gets serialized to/from JSON.

#### Schema Versioning

The `schema_version` field tracks the settings file format version.
When the settings structure changes, increment the version and handle migration in load_or_create().


## Variables

| Name | Description |
| ---- | ----------- |
| [schema_version](#schema_version) | Settings file format version for migration support  |
| [general](#general) | General application settings  |
| [audio](#audio) | Audio engine settings  |

## Variable Details

### audio<a name="audio"></a>

!!! variable "Audio audio"

    Audio engine settings
    

### general<a name="general"></a>

!!! variable "General general"

    General application settings
    

### schema_version<a name="schema_version"></a>

!!! variable "int schema_version"

    Settings file format version for migration support
    

