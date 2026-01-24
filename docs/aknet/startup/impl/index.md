---
generator: doxide
---


# impl

Placeholder and test step implementations.


## Types

| Name | Description |
| ---- | ----------- |
| [CheckJackInstallationStep](CheckJackInstallationStep.md) | Simulates checking for JACK audio server installation. |
| [ConnectToRegistryStep](ConnectToRegistryStep.md) | Simulates connecting to an NMOS registry. |
| [DiscoverNMOSRegistryStep](DiscoverNMOSRegistryStep.md) | Simulates discovering an NMOS registry on the network. |
| [FailingStep](FailingStep.md) | A step that always fails. |
| [FakeStep](FakeStep.md) | Base class for simulated startup steps. |
| [InitializeAudioEngineStep](InitializeAudioEngineStep.md) | Simulates initializing the audio engine. |
| [LoadAudioDevicesStep](LoadAudioDevicesStep.md) | Simulates loading available audio devices. |
| [LoadUserPreferencesStep](LoadUserPreferencesStep.md) | Simulates loading user preferences. |
| [SlowStep](SlowStep.md) | A step that takes longer than its timeout. |

## Functions

| Name | Description |
| ---- | ----------- |
| [create_default_steps](#create_default_steps) | Create the default startup step sequence. |

## Function Details

### create_default_steps<a name="create_default_steps"></a>
!!! function "inline std::vector&lt;std::unique_ptr&lt;IStartupStep&gt;&gt; create_default_steps()"

    Create the default startup step sequence.
    
    Returns the standard set of placeholder steps in the correct execution order.
    
    
    :material-keyboard-return: **Return**
    :    Vector of startup steps ready to be passed to StartupManager::set_steps().
    
    #### Steps Included
    
    1. CheckJackInstallationStep (critical)
    2. DiscoverNMOSRegistryStep (critical)
    3. ConnectToRegistryStep (critical)
    4. LoadAudioDevicesStep (critical)
    5. InitializeAudioEngineStep (critical)
    6. LoadUserPreferencesStep (non-critical)
    

