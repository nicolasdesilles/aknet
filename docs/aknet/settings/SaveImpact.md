---
generator: doxide
---


# SaveImpact

**struct SaveImpact**

Describes the restart impact of a save() operation.

Returned by Settings::save() to inform the caller what actions are needed for changes to take effect.


## Variables

| Name | Description |
| ---- | ----------- |
| [app_restart_required](#app_restart_required) | True if a full app restart is needed  |
| [modules_restart_required](#modules_restart_required) | List of module names that need restart  |
| [restart_sensitive_keys_changed](#restart_sensitive_keys_changed) | List of setting keys that triggered restart requirements  |

## Variable Details

### app_restart_required<a name="app_restart_required"></a>

!!! variable "bool app_restart_required"

    True if a full app restart is needed
    

### modules_restart_required<a name="modules_restart_required"></a>

!!! variable "std::vector&lt;std::string&gt; modules_restart_required"

    List of module names that need restart
    

### restart_sensitive_keys_changed<a name="restart_sensitive_keys_changed"></a>

!!! variable "std::vector&lt;std::string&gt; restart_sensitive_keys_changed"

    List of setting keys that triggered restart requirements
    

