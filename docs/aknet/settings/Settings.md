---
generator: doxide
---


# Settings

**class Settings**

Thread-safe application settings manager with staged editing.

Settings provides a centralized way to manage application configuration with the following features:

- **Immutable snapshots** for thread-safe read access from any thread
- **Staged editing** where changes are made to a pending copy before saving
- **Restart impact tracking** to determine what needs restarting after changes
- **Atomic file persistence** to prevent corruption

#### Lifecycle

1. Create a Settings instance
2. Call init() with a logger and configuration
3. Call load_or_create() to load existing settings or create defaults
4. Use snapshot() to read settings, stage() to modify pending settings
5. Call save() to persist changes
6. Call shutdown() before destruction

#### Thread Safety

- snapshot() returns an immutable shared_ptr that can be safely read from any thread
- stage(), save(), and other mutating operations are protected by an internal mutex
- Multiple threads can hold snapshot references simultaneously

#### Example

```cpp
auto settings = std::make_unique<Settings>();
settings->init(logger, config);
settings->load_or_create();

// Read current settings (thread-safe)
auto snap = settings->snapshot();
int sample_rate = snap->audio.sampling_rate;

// Modify settings (staged)
settings->stage([](AppSettings& s) {
    s.audio.sampling_rate = 96000;
});

// Save changes and check impact
auto [result, impact] = settings->save();
if (impact.modules_restart_required.contains("audio")) {
    // Restart audio engine
}
```


## Types

| Name | Description |
| ---- | ----------- |
| [SaveResult](Settings/SaveResult.md) | Result of a save operation including restart impact information. |

## Variables

| Name | Description |
| ---- | ----------- |
| [pending_mutex_](#pending_mutex_) | Protects pending_ and restart_rules_. |
| [restart_rules_](#restart_rules_) | Registered restart rules. |
| [defaults_](#defaults_) | Default settings (used for reset and initial creation). |
| [pending_](#pending_) | Pending changes (staged but not yet saved). |
| [snapshot_](#snapshot_) | Immutable active settings snapshot. |
| [config_](#config_) | Configuration (file path, schema version). |
| [initialized_](#initialized_) | True if init() has been called. |
| [logger_](#logger_) | Logger for diagnostic output. |

## Functions

| Name | Description |
| ---- | ----------- |
| [Settings](#Settings) | Default constructor. |
| [~Settings](#_u007eSettings) | Destructor. |
| [init](#init) | Initialize the settings system. |
| [shutdown](#shutdown) | Shutdown the settings system. |
| [is_initialized](#is_initialized) | Check if the settings system is initialized. |
| [has_pending_changes](#has_pending_changes) | Check if there are unsaved pending changes. |
| [add_restart_rule](#add_restart_rule) | Register a restart rule for a setting key. |
| [get_restart_rules](#get_restart_rules) | Get all registered restart rules. |
| [path](#path) | Get the full path to the settings file. |
| [snapshot](#snapshot) | Get an immutable snapshot of the current active settings. |
| [load_or_create](#load_or_create) | Load settings from file or create with defaults. |
| [pending_copy](#pending_copy) | Get a mutable copy of the pending settings. |
| [stage](#stage) | Modify the pending settings using a mutator function. |
| [reset_pending_to_active](#reset_pending_to_active) | Discard pending changes and reset to the active snapshot. |
| [save](#save) | Save pending settings to file and update the active snapshot. |
| [export_to_file](#export_to_file) | Export current active settings to an arbitrary file. |
| [import_from_file](#import_from_file) | Import settings from a file into pending. |

## Variable Details

### config_<a name="config_"></a>

!!! variable "SettingsConfig config_"

    Configuration (file path, schema version).
    

### defaults_<a name="defaults_"></a>

!!! variable "AppSettings defaults_"

    Default settings (used for reset and initial creation).
    

### initialized_<a name="initialized_"></a>

!!! variable "bool initialized_"

    True if init() has been called.
    

### logger_<a name="logger_"></a>

!!! variable "std::shared_ptr&lt;log::Logger&gt; logger_"

    Logger for diagnostic output.
    

### pending_<a name="pending_"></a>

!!! variable "AppSettings pending_"

    Pending changes (staged but not yet saved).
    

### pending_mutex_<a name="pending_mutex_"></a>

!!! variable "mutable std::mutex pending_mutex_"

    Protects pending_ and restart_rules_.
    

### restart_rules_<a name="restart_rules_"></a>

!!! variable "std::vector&lt;RestartRule&gt; restart_rules_"

    Registered restart rules.
    

### snapshot_<a name="snapshot_"></a>

!!! variable "std::shared_ptr&lt;const AppSettings&gt; snapshot_"

    Immutable active settings snapshot.
    

## Function Details

### Settings<a name="Settings"></a>
!!! function "Settings()"

    Default constructor.
    
    Creates an uninitialized Settings instance.
    Call init() before using any other methods.
    

### add_restart_rule<a name="add_restart_rule"></a>
!!! function "void add_restart_rule(RestartRule rule)"

    Register a restart rule for a setting key.
    
    Restart rules define what happens when specific settings change.
    When save() is called, it checks all registered rules against the changed keys.
    
    
    :material-location-enter: `rule`
    :    The restart rule to register.
    
    #### Example
    
    ```cpp
    settings->add_restart_rule({
        .key = "audio.sampling_rate",
        .requires_app_restart = false,
        .module_name_to_restart = "audio"
    });
    ```
    

### export_to_file<a name="export_to_file"></a>
!!! function "Result export_to_file(const std::filesystem::path&amp; file_path)"

    Export current active settings to an arbitrary file.
    
    Useful for creating backups or sharing configurations.
    
    
    :material-location-enter: `file_path`
    :    Destination file path.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or failure.
    

### get_restart_rules<a name="get_restart_rules"></a>
!!! function "std::vector&lt;RestartRule&gt; get_restart_rules()"

    Get all registered restart rules.
    
    
    :material-keyboard-return: **Return**
    :    Copy of the restart rules vector.
    

### has_pending_changes<a name="has_pending_changes"></a>
!!! function "bool has_pending_changes()"

    Check if there are unsaved pending changes.
    
    Compares the pending settings against the active snapshot.
    
    
    :material-keyboard-return: **Return**
    :    True if pending settings differ from the active snapshot.
    

### import_from_file<a name="import_from_file"></a>
!!! function "Result import_from_file(const std::filesystem::path&amp; file_path)"

    Import settings from a file into pending.
    
    Loads settings from the specified file into the pending copy.
    The imported settings are not active until save() is called.
    
    
    :material-location-enter: `file_path`
    :    Source file path.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or failure.
    
    
    !!! note
    
        This only updates pending settings, not the active snapshot.
        Call save() after import to make the changes active.
    

### init<a name="init"></a>
!!! function "void init(std::shared_ptr&lt;log::Logger&gt; logger, SettingsConfig config)"

    Initialize the settings system.
    
    Must be called before any other methods.
    Sets up the logger, configuration, and creates an initial snapshot with default values.
    
    
    :material-location-enter: `logger`
    :    Logger instance for diagnostic output.
        
    :material-location-enter: `config`
    :    Configuration specifying file location and schema version.
    
    #### Exceptions
    
    - Throws `std::invalid_argument` if logger is null.
    - Throws `std::invalid_argument` if config.base_dir is empty.
    

### is_initialized<a name="is_initialized"></a>
!!! function "bool is_initialized()"

    Check if the settings system is initialized.
    
    
    :material-keyboard-return: **Return**
    :    True if init() has been called and shutdown() has not.
    

### load_or_create<a name="load_or_create"></a>
!!! function "Result load_or_create()"

    Load settings from file or create with defaults.
    
    If the settings file exists, loads and validates it.
    If the file does not exist, creates it with default values.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or failure.
    
    
    !!! note
    
        This should be called after init() to load persisted settings.
        On success, both snapshot and pending are updated to the loaded values.
    

### path<a name="path"></a>
!!! function "std::filesystem::path path()"

    Get the full path to the settings file.
    
    
    :material-keyboard-return: **Return**
    :    Combined path of base_dir and file_name from configuration.
    

### pending_copy<a name="pending_copy"></a>
!!! function "AppSettings pending_copy()"

    Get a mutable copy of the pending settings.
    
    
    :material-keyboard-return: **Return**
    :    Copy of the pending AppSettings struct.
    
    
    !!! note
    
        This returns a copy, not a reference.
        To modify pending settings, use stage() instead.
    

### reset_pending_to_active<a name="reset_pending_to_active"></a>
!!! function "Result reset_pending_to_active()"

    Discard pending changes and reset to the active snapshot.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or failure.
    

### save<a name="save"></a>
!!! function "SaveResult save()"

    Save pending settings to file and update the active snapshot.
    
    Writes the pending settings to disk using atomic file operations.
    Compares old and new settings against restart rules to determine impact.
    Updates the active snapshot to the newly saved values.
    
    
    :material-keyboard-return: **Return**
    :    SaveResult containing success status and restart impact information.
    
    #### Example
    
    ```cpp
    auto [result, impact] = settings->save();
    if (!result.ok) {
        logger->error("Failed to save: {}", result.error);
        return;
    }
    if (impact.app_restart_required) {
        // Prompt user to restart application
    }
    for (const auto& module : impact.modules_restart_required) {
        // Restart the affected module
    }
    ```
    

### shutdown<a name="shutdown"></a>
!!! function "void shutdown()"

    Shutdown the settings system.
    
    Releases all resources and resets to uninitialized state.
    Any unsaved pending changes are discarded.
    
    
    !!! warning
    
        Does not save pending changes.
        Call save() before shutdown() if you want to persist changes.
    

### snapshot<a name="snapshot"></a>
!!! function "std::shared_ptr&lt;const AppSettings&gt; snapshot()"

    Get an immutable snapshot of the current active settings.
    
    The returned pointer is thread-safe to read from any thread.
    The snapshot remains valid even if the settings are modified and saved.
    
    
    :material-keyboard-return: **Return**
    :    Shared pointer to immutable AppSettings.
    
    
    !!! note
    
        The snapshot is replaced when save() is called.
        Callers holding old snapshots will still see the old values.
    

### stage<a name="stage"></a>
!!! function "Result stage(std::function&lt;void(AppSettings&amp;)&gt; mutator)"

    Modify the pending settings using a mutator function.
    
    The mutator receives a mutable reference to the pending settings.
    Changes are not persisted until save() is called.
    
    
    :material-location-enter: `mutator`
    :    Function that modifies the pending settings.
    
    
    :material-keyboard-return: **Return**
    :    Result (currently always succeeds).
    
    #### Example
    
    ```cpp
    settings->stage([](AppSettings& s) {
        s.general.log_level = "trace";
        s.audio.buffer_size = 512;
    });
    ```
    

### ~Settings<a name="_u007eSettings"></a>
!!! function "~Settings()"

    Destructor.
    

