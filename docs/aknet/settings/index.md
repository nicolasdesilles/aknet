---
generator: doxide
---


# settings

Persistent application settings module with staged editing.

This util provides a centralized, thread-safe settings system for the app.
This util is meant to be owned by the core.

Settings are stored as JSON and loaded/saved from disk.
The system uses a staged editing pattern where changes are made to a pending copy and explicitly saved.


!!! info

    The other modules in the application will not create their own settings instance.
    Instead, the core will own the Settings instance and provide read access to modules via immutable snapshots.

## Design Goals

- **Thread Safety**: Read access via immutable snapshots, write access protected by mutex.
- **Staged Editing**: Changes are staged in a pending copy, allowing preview and discard before saving.
- **Restart Impact**: Rules determine which settings changes require module or application restarts.
- **Atomic Persistence**: File writes use temp files and backups to prevent corruption.
- **JSON Format**: Human-readable settings file using nlohmann::json.


## Types

| Name | Description |
| ---- | ----------- |
| [AppSettings](AppSettings.md) | Root application settings structure. |
| [Audio](Audio.md) | Audio engine settings. |
| [General](General.md) | General application settings. |
| [RestartImpact](RestartImpact.md) | Categorizes the type of restart needed after a settings change. |
| [RestartRule](RestartRule.md) | Defines the restart behavior required when a specific setting changes. |
| [Result](Result.md) | Simple result type for operations that can fail. |
| [SaveImpact](SaveImpact.md) | Describes the restart impact of a save() operation. |
| [Settings](Settings.md) | Thread-safe application settings manager with staged editing. |
| [SettingsConfig](SettingsConfig.md) | Configuration for initializing the Settings system. |

## Functions

| Name | Description |
| ---- | ----------- |
| [from_json_file](#from_json_file) | Load AppSettings from a JSON file. |
| [from_json_string](#from_json_string) | Parse a JSON string into an AppSettings struct. |
| [to_json_file](#to_json_file) | Save AppSettings to a JSON file with atomic write. |
| [to_json_string](#to_json_string) | Serialize an AppSettings struct to a JSON string. |

## Function Details

### from_json_file<a name="from_json_file"></a>
!!! function "Result from_json_file(const std::filesystem::path&amp; file_path, AppSettings&amp; out)"

    Load AppSettings from a JSON file.
    
    
    :material-location-enter: `file_path`
    :    Path to the JSON file.
        
    :material-location-exit: `out`
    :    The AppSettings struct to populate.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or failure with error message.
    

### from_json_string<a name="from_json_string"></a>
!!! function "Result from_json_string(std::string_view json_str, AppSettings&amp; out)"

    Parse a JSON string into an AppSettings struct.
    
    
    :material-location-enter: `json_str`
    :    The JSON string to parse.
        
    :material-location-exit: `out`
    :    The AppSettings struct to populate.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or failure with error message.
    

### to_json_file<a name="to_json_file"></a>
!!! function "Result to_json_file(const std::filesystem::path&amp; file_path, const AppSettings&amp; settings)"

    Save AppSettings to a JSON file with atomic write.
    
    Uses a safe write pattern: writes to a temp file, backs up the existing file, then renames.
    This prevents data loss if the write is interrupted.
    
    
    :material-location-enter: `file_path`
    :    Path to the JSON file.
        
    :material-location-enter: `settings`
    :    The settings to save.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or failure with error message.
    

### to_json_string<a name="to_json_string"></a>
!!! function "std::string to_json_string(const AppSettings&amp; settings)"

    Serialize an AppSettings struct to a JSON string.
    
    
    :material-location-enter: `settings`
    :    The settings to serialize.
    
    
    :material-keyboard-return: **Return**
    :    JSON string representation of the settings.
    

