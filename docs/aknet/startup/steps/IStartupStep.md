---
generator: doxide
---


# IStartupStep

**class IStartupStep**

Abstract interface for startup steps.

All startup steps must implement this interface.
Steps are executed sequentially by the StartupEngine.

#### Implementing a Custom Step

1. Inherit from IStartupStep
2. Store a StepConfig in your class
3. Implement config() to return your configuration
4. Implement run() with your step's logic

#### Example

```cpp
class CheckJackInstallationStep : public IStartupStep {
    StepConfig config_{
        .id = "check_jack",
        .display_name = "Check JACK Installation",
        .timeout = std::chrono::seconds{5},
        .critical = true,
        .can_skip = false
    };

public:
    const StepConfig& config() const override {
        return config_;
    }

    StepResult run(StepContext& ctx) override {
        ctx.logger->info("Checking JACK installation...");

        if (!is_jack_installed()) {
            return {StepStatus::Failed, "JACK is not installed"};
        }

        return {StepStatus::Success, "JACK found"};
    }
};
```


