//
// Created by Nicolas Désilles on 14/01/2026.
//
// Fake/placeholder startup steps for end-to-end testing.
// These simulate real operations with configurable delays and outcomes.
//

#ifndef AKNET_STEPS_DEFINITION_H
#define AKNET_STEPS_DEFINITION_H

#pragma once

#include <startup_step.h>
#include <thread>

namespace aknet::startup {

    // Base class for fake steps with common delay logic
    class FakeStep : public IStartupStep {
    protected:
        StepConfig config_;
        std::chrono::milliseconds delay_;
        StepStatus outcome_;
        std::string outcome_message_;

    public:
        FakeStep(StepConfig config, 
                 std::chrono::milliseconds delay,
                 StepStatus outcome = StepStatus::Success,
                 std::string outcome_message = "")
            : config_(std::move(config))
            , delay_(delay)
            , outcome_(outcome)
            , outcome_message_(std::move(outcome_message))
        {}

        const StepConfig& config() const override {
            return config_;
        }

        StepResult run(StepContext& ctx) override {
            if (ctx.logger) {
                ctx.logger->info("[{}] Starting (delay: {}ms)", config_.id, delay_.count());
            }

            // Simulate work with interruptible delay
            auto end_time = std::chrono::steady_clock::now() + delay_;
            while (std::chrono::steady_clock::now() < end_time) {
                if (ctx.abort_requested()) {
                    if (ctx.logger) {
                        ctx.logger->info("[{}] Aborted", config_.id);
                    }
                    return {StepStatus::Aborted, "Step was aborted"};
                }
                
                if (ctx.is_expired()) {
                    if (ctx.logger) {
                        ctx.logger->warn("[{}] Timed out", config_.id);
                    }
                    return {StepStatus::TimedOut, "Step exceeded timeout"};
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (ctx.logger) {
                ctx.logger->info("[{}] Completed with status: {}", 
                    config_.id, static_cast<int>(outcome_));
            }

            return {outcome_, outcome_message_};
        }
    };

    // -------------------------------------------------------------------------
    // Concrete Fake Steps
    // -------------------------------------------------------------------------

    class CheckJackInstallationStep : public FakeStep {
    public:
        CheckJackInstallationStep()
            : FakeStep(
                StepConfig{
                    .id = "check_jack",
                    .display_name = "Check JACK Installation",
                    .timeout = std::chrono::seconds(5),
                    .critical = true,
                    .can_skip = false
                },
                std::chrono::milliseconds(800)
            )
        {}
    };

    class DiscoverNMOSRegistryStep : public FakeStep {
    public:
        DiscoverNMOSRegistryStep()
            : FakeStep(
                StepConfig{
                    .id = "discover_nmos",
                    .display_name = "Discover NMOS Registry",
                    .timeout = std::chrono::seconds(10),
                    .critical = true,
                    .can_skip = false
                },
                std::chrono::milliseconds(1200)
            )
        {}
    };

    class ConnectToRegistryStep : public FakeStep {
    public:
        ConnectToRegistryStep()
            : FakeStep(
                StepConfig{
                    .id = "connect_registry",
                    .display_name = "Connect to Registry",
                    .timeout = std::chrono::seconds(5),
                    .critical = true,
                    .can_skip = false
                },
                std::chrono::milliseconds(600)
            )
        {}
    };

    class LoadAudioDevicesStep : public FakeStep {
    public:
        LoadAudioDevicesStep()
            : FakeStep(
                StepConfig{
                    .id = "load_audio_devices",
                    .display_name = "Load Audio Devices",
                    .timeout = std::chrono::seconds(5),
                    .critical = true,
                    .can_skip = false
                },
                std::chrono::milliseconds(500)
            )
        {}
    };

    class InitializeAudioEngineStep : public FakeStep {
    public:
        InitializeAudioEngineStep()
            : FakeStep(
                StepConfig{
                    .id = "init_audio_engine",
                    .display_name = "Initialize Audio Engine",
                    .timeout = std::chrono::seconds(3),
                    .critical = true,
                    .can_skip = false
                },
                std::chrono::milliseconds(400)
            )
        {}
    };

    class LoadUserPreferencesStep : public FakeStep {
    public:
        LoadUserPreferencesStep()
            : FakeStep(
                StepConfig{
                    .id = "load_preferences",
                    .display_name = "Load User Preferences",
                    .timeout = std::chrono::seconds(2),
                    .critical = false,
                    .can_skip = true
                },
                std::chrono::milliseconds(300)
            )
        {}
    };

    // -------------------------------------------------------------------------
    // Special test steps (for testing failure/timeout scenarios)
    // -------------------------------------------------------------------------

    class FailingStep : public FakeStep {
    public:
        FailingStep()
            : FakeStep(
                StepConfig{
                    .id = "failing_step",
                    .display_name = "Failing Step (Test)",
                    .timeout = std::chrono::seconds(5),
                    .critical = true,
                    .can_skip = false
                },
                std::chrono::milliseconds(500),
                StepStatus::Failed,
                "Simulated failure for testing"
            )
        {}
    };

    class SlowStep : public FakeStep {
    public:
        SlowStep()
            : FakeStep(
                StepConfig{
                    .id = "slow_step",
                    .display_name = "Slow Step (Test)",
                    .timeout = std::chrono::seconds(2),
                    .critical = false,
                    .can_skip = true
                },
                std::chrono::milliseconds(5000)  // Will timeout (5s > 2s timeout)
            )
        {}
    };

    // -------------------------------------------------------------------------
    // Helper to create the default step sequence
    // -------------------------------------------------------------------------

    inline std::vector<std::unique_ptr<IStartupStep>> create_default_steps() {
        std::vector<std::unique_ptr<IStartupStep>> steps;
        
        steps.push_back(std::make_unique<CheckJackInstallationStep>());
        steps.push_back(std::make_unique<DiscoverNMOSRegistryStep>());
        steps.push_back(std::make_unique<ConnectToRegistryStep>());
        steps.push_back(std::make_unique<LoadAudioDevicesStep>());
        steps.push_back(std::make_unique<InitializeAudioEngineStep>());
        steps.push_back(std::make_unique<LoadUserPreferencesStep>());
        
        return steps;
    }

} // namespace aknet::startup

#endif // AKNET_STEPS_DEFINITION_H
