---
generator: doxide
---


# json

JSON serialization for startup types.


## Functions

| Name | Description |
| ---- | ----------- |
| [deserialize_progress](#deserialize_progress) | Deserialize SequenceProgress from a JSON string. |
| [deserialize_result](#deserialize_result) | Deserialize Result from a JSON string. |
| [deserialize_step_config](#deserialize_step_config) | Deserialize StepConfig from a JSON string. |
| [from_json](#from_json) | Deserialize AppState from JSON. |
| [from_json](#from_json) | Deserialize StepStatus from JSON. |
| [from_json](#from_json) | Deserialize AbortReason from JSON. |
| [from_json](#from_json) | Deserialize StepConfig from JSON. |
| [from_json](#from_json) | Deserialize StepProgress from JSON. |
| [from_json](#from_json) | Deserialize SequenceProgress from JSON. |
| [from_json](#from_json) | Deserialize Result from JSON. |
| [serialize_progress](#serialize_progress) | Serialize SequenceProgress to a JSON string. |
| [serialize_result](#serialize_result) | Serialize Result to a JSON string. |
| [serialize_step_config](#serialize_step_config) | Serialize StepConfig to a JSON string. |
| [to_json](#to_json) | Serialize AppState to JSON (as integer). |
| [to_json](#to_json) | Serialize StepStatus to JSON (as integer). |
| [to_json](#to_json) | Serialize AbortReason to JSON (as integer). |
| [to_json](#to_json) | Serialize StepConfig to JSON. |
| [to_json](#to_json) | Serialize StepProgress to JSON. |
| [to_json](#to_json) | Serialize SequenceProgress to JSON. |
| [to_json](#to_json) | Serialize Result to JSON. |

## Function Details

### deserialize_progress<a name="deserialize_progress"></a>
!!! function "Result deserialize_progress(const std::string&amp; json_str, SequenceProgress&amp; out)"

    Deserialize SequenceProgress from a JSON string.
    
    
    :material-location-enter: `json_str`
    :    JSON string to parse.
        
    :material-location-exit: `out`
    :    SequenceProgress to populate.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or parse error.
    

### deserialize_result<a name="deserialize_result"></a>
!!! function "Result deserialize_result(const std::string&amp; json_str, Result&amp; out)"

    Deserialize Result from a JSON string.
    
    
    :material-location-enter: `json_str`
    :    JSON string to parse.
        
    :material-location-exit: `out`
    :    Result to populate.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or parse error.
    

### deserialize_step_config<a name="deserialize_step_config"></a>
!!! function "Result deserialize_step_config(const std::string&amp; json_str, StepConfig&amp; out)"

    Deserialize StepConfig from a JSON string.
    
    
    :material-location-enter: `json_str`
    :    JSON string to parse.
        
    :material-location-exit: `out`
    :    StepConfig to populate.
    
    
    :material-keyboard-return: **Return**
    :    Result indicating success or parse error.
    

### from_json<a name="from_json"></a>
!!! function "void from_json(const nlohmann::json&amp; j, AppState&amp; state)"

    Deserialize AppState from JSON.
    

!!! function "void from_json(const nlohmann::json&amp; j, StepStatus&amp; status)"

    Deserialize StepStatus from JSON.
    

!!! function "void from_json(const nlohmann::json&amp; j, AbortReason&amp; reason)"

    Deserialize AbortReason from JSON.
    

!!! function "void from_json(const nlohmann::json&amp; j, StepConfig&amp; config)"

    Deserialize StepConfig from JSON.
    

!!! function "void from_json(const nlohmann::json&amp; j, StepProgress&amp; progress)"

    Deserialize StepProgress from JSON.
    

!!! function "void from_json(const nlohmann::json&amp; j, SequenceProgress&amp; progress)"

    Deserialize SequenceProgress from JSON.
    

!!! function "void from_json(const nlohmann::json&amp; j, Result&amp; result)"

    Deserialize Result from JSON.
    

### serialize_progress<a name="serialize_progress"></a>
!!! function "std::string serialize_progress(const SequenceProgress&amp; progress)"

    Serialize SequenceProgress to a JSON string.
    
    
    :material-location-enter: `progress`
    :    Progress to serialize.
    
    
    :material-keyboard-return: **Return**
    :    JSON string representation.
    

### serialize_result<a name="serialize_result"></a>
!!! function "std::string serialize_result(const Result&amp; result)"

    Serialize Result to a JSON string.
    
    
    :material-location-enter: `result`
    :    Result to serialize.
    
    
    :material-keyboard-return: **Return**
    :    JSON string representation.
    

### serialize_step_config<a name="serialize_step_config"></a>
!!! function "std::string serialize_step_config(const StepConfig&amp; config)"

    Serialize StepConfig to a JSON string.
    
    
    :material-location-enter: `config`
    :    Config to serialize.
    
    
    :material-keyboard-return: **Return**
    :    JSON string representation.
    

### to_json<a name="to_json"></a>
!!! function "void to_json(nlohmann::json&amp; j, AppState state)"

    Serialize AppState to JSON (as integer).
    

!!! function "void to_json(nlohmann::json&amp; j, StepStatus status)"

    Serialize StepStatus to JSON (as integer).
    

!!! function "void to_json(nlohmann::json&amp; j, AbortReason reason)"

    Serialize AbortReason to JSON (as integer).
    

!!! function "void to_json(nlohmann::json&amp; j, const StepConfig&amp; config)"

    Serialize StepConfig to JSON.
    

!!! function "void to_json(nlohmann::json&amp; j, const StepProgress&amp; progress)"

    Serialize StepProgress to JSON.
    

!!! function "void to_json(nlohmann::json&amp; j, const SequenceProgress&amp; progress)"

    Serialize SequenceProgress to JSON.
    

!!! function "void to_json(nlohmann::json&amp; j, const Result&amp; result)"

    Serialize Result to JSON.
    

