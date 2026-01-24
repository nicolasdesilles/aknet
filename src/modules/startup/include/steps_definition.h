//
// steps_definition.h - Placeholder startup step implementations.
//

#ifndef AKNET_STEPS_DEFINITION_H
#define AKNET_STEPS_DEFINITION_H

#pragma once

#include <startup_step.h>
#include <thread>

namespace aknet::startup {

    /**
     * Placeholder and test step implementations.
     */
    namespace impl {

        /**
         * Base class for simulated startup steps.
     *
     * Provides a common implementation that simulates work with a configurable delay.
     * Checks for abort and timeout during the simulated work period.
     *
     * @warning
     * These are placeholder implementations!
     * Replace with real implementations before production use.
     *
     * #### Behavior
     *
     * 1. Logs step start
     * 2. Sleeps in 50ms increments, checking abort/timeout each iteration
     * 3. Returns the configured outcome when delay completes
     */
    class FakeStep : public IStartupStep {
    protected:
        /**
         * Step configuration.
         */
        StepConfig config_;

        /**
         * Simulated execution time.
         */
        std::chrono::milliseconds delay_;

        /**
         * Status to return after delay.
         */
        StepStatus outcome_;

        /**
         * Message to return with the outcome.
         */
        std::string outcome_message_;

    public:
        /**
         * Construct a FakeStep with configuration and behavior.
         *
         * @param config Step configuration.
         * @param delay Simulated execution time.
         * @param outcome Status to return (default: Success).
         * @param outcome_message Message to return (default: empty).
         */
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

    // =========================================================================
    // Concrete Placeholder Steps
    // =========================================================================

    /**
     * Simulates checking for JACK audio server installation.
     *
     * In production: Should verify JACK is installed and accessible.
     */
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

    /**
     * Simulates discovering an NMOS registry on the network.
     *
     * In production: Should use mDNS/DNS-SD to find NMOS registries.
     */
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

    /**
     * Simulates connecting to an NMOS registry.
     *
     * In production: Should establish connection to the discovered registry.
     */
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

    /**
     * Simulates loading available audio devices.
     *
     * In production: Should enumerate JACK ports and audio interfaces.
     */
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

    /**
     * Simulates initializing the audio engine.
     *
     * In production: Should initialize JACK client, set up audio callbacks.
     */
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

    /**
     * Simulates loading user preferences.
     *
     * This is a non-critical step that can be skipped if it fails.
     * In production: Should load saved user preferences and apply them.
     */
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

    // =========================================================================
    // Test Steps
    // =========================================================================

    /**
     * A step that always fails.
     *
     * Used for testing failure handling and retry logic.
     */
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

    /**
     * A step that takes longer than its timeout.
     *
     * Used for testing timeout handling.
     * Has a 2-second timeout but takes 5 seconds to complete.
     */
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

    // =========================================================================
    // Factory Function
    // =========================================================================

    /**
     * Create the default startup step sequence.
     *
     * Returns the standard set of placeholder steps in the correct execution order.
     *
     * @return Vector of startup steps ready to be passed to StartupManager::set_steps().
     *
     * #### Steps Included
     *
     * 1. CheckJackInstallationStep (critical)
     * 2. DiscoverNMOSRegistryStep (critical)
     * 3. ConnectToRegistryStep (critical)
     * 4. LoadAudioDevicesStep (critical)
     * 5. InitializeAudioEngineStep (critical)
     * 6. LoadUserPreferencesStep (non-critical)
     */
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

    } // namespace impl

    // Re-export step implementations at startup:: level for API convenience
    using impl::FakeStep;
    using impl::CheckJackInstallationStep;
    using impl::DiscoverNMOSRegistryStep;
    using impl::ConnectToRegistryStep;
    using impl::LoadAudioDevicesStep;
    using impl::InitializeAudioEngineStep;
    using impl::LoadUserPreferencesStep;
    using impl::FailingStep;
    using impl::SlowStep;
    using impl::create_default_steps;

} // namespace aknet::startup

#endif // AKNET_STEPS_DEFINITION_H
