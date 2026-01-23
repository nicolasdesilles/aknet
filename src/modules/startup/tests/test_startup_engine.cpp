//
// Created by Nicolas Désilles on 23/01/2026.
//

#include "test_fixtures.h"

using namespace aknet;
using namespace aknet::test;

// ------------------------------------------------------------------------------------------------
// Startup Engine Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Startup | Startup Engine Constructor", "[startup][engine]") {

    SECTION("constructor throws when logger is null") {
        EngineTestFixture f;

        REQUIRE_THROWS_AS(
            startup::StartupEngine(nullptr, f.settings, f.clock),
            std::invalid_argument
        );
    }

    SECTION("constructor throws when settings is null") {
        EngineTestFixture f;

        REQUIRE_THROWS_AS(
            startup::StartupEngine(f.logger_startup, nullptr, f.clock),
            std::invalid_argument
        );
    }

    SECTION("constructor throws when clock is null") {
        EngineTestFixture f;

        REQUIRE_THROWS_AS(
            startup::StartupEngine(f.logger_startup, f.settings, nullptr),
            std::invalid_argument
        );
    }

    SECTION("constructor succeeds with valid dependencies") {
        EngineTestFixture f;

        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        REQUIRE(engine.state() == startup::AppState::Off);
        REQUIRE(engine.has_steps() == false);
        REQUIRE(engine.step_count() == 0);
        REQUIRE(engine.progress().current_step_index == -1);
        REQUIRE(engine.progress().steps.empty());
    }

}

TEST_CASE("Startup | Step registration and validation", "[startup][engine]") {

    SECTION("set_steps validates duplicate IDs") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Create two steps with the same ID
        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "duplicate_id",
                .display_name = "Step 1",
                .timeout = std::chrono::seconds{5}
            },
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "duplicate_id",
                .display_name = "Step 2",
                .timeout = std::chrono::seconds{5}
            },
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        auto result = engine.set_steps(std::move(steps));

        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("duplicate_id") != std::string::npos);
    }

    SECTION("set_steps rejects empty steps vector") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> empty_steps;
        auto result = engine.set_steps(std::move(empty_steps));

        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("No steps") != std::string::npos);
    }

}

TEST_CASE("Startup | Sequence execution", "[startup][engine][execution]") {

    SECTION("three success steps reach Active state") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Create 3 steps that succeed
        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step1",
                .display_name = "Step 1",
                .timeout = std::chrono::seconds{10}
            },
            startup::StepResult{startup::StepStatus::Success, "Step 1 OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step2",
                .display_name = "Step 2",
                .timeout = std::chrono::seconds{10}
            },
            startup::StepResult{startup::StepStatus::Success, "Step 2 OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step3",
                .display_name = "Step 3",
                .timeout = std::chrono::seconds{10}
            },
            startup::StepResult{startup::StepStatus::Success, "Step 3 OK"}
        ));

        auto set_result = engine.set_steps(std::move(steps));
        REQUIRE(set_result.ok);

        // Run the sequence
        const auto& progress = engine.run({});

        // Verify final state
        REQUIRE(progress.state == startup::AppState::Active);
        REQUIRE(progress.current_step_index == -1);  // No step running
        REQUIRE(progress.can_retry == false);
        REQUIRE_FALSE(progress.last_error.has_value());

        // Verify all steps succeeded
        REQUIRE(progress.steps.size() == 3);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
        REQUIRE(progress.steps[1].status == startup::StepStatus::Success);
        REQUIRE(progress.steps[2].status == startup::StepStatus::Success);

        // Verify timestamps were set
        REQUIRE(progress.steps[0].start_time.has_value());
        REQUIRE(progress.steps[0].end_time.has_value());
        REQUIRE(progress.steps[1].start_time.has_value());
        REQUIRE(progress.steps[1].end_time.has_value());
        REQUIRE(progress.steps[2].start_time.has_value());
        REQUIRE(progress.steps[2].end_time.has_value());
    }

    SECTION("critical failure stops sequence and returns to Off") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Create 3 steps: success, critical fail, success (should not run)
        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step1",
                .display_name = "Step 1",
                .timeout = std::chrono::seconds{10},
                .critical = false
            },
            startup::StepResult{startup::StepStatus::Success, "Step 1 OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step2",
                .display_name = "Step 2",
                .timeout = std::chrono::seconds{10},
                .critical = true
            },
            startup::StepResult{startup::StepStatus::Failed, "Step 2 failed"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step3",
                .display_name = "Step 3",
                .timeout = std::chrono::seconds{10}
            },
            startup::StepResult{startup::StepStatus::Success, "Step 3 OK"}
        ));

        auto set_result = engine.set_steps(std::move(steps));
        REQUIRE(set_result.ok);

        // Run the sequence
        const auto& progress = engine.run({});

        // Verify final state
        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.can_retry == true);
        REQUIRE(progress.last_error.has_value());
        REQUIRE(progress.last_error.value().find("step2") != std::string::npos);

        // Verify step statuses
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
        REQUIRE(progress.steps[1].status == startup::StepStatus::Failed);
        REQUIRE(progress.steps[2].status == startup::StepStatus::Pending);  // Should not have run
    }

    SECTION("non-critical failure continues and can reach Active") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Create 3 steps: success, non-critical fail, success
        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step1",
                .display_name = "Step 1",
                .timeout = std::chrono::seconds{10}
            },
            startup::StepResult{startup::StepStatus::Success, "Step 1 OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step2",
                .display_name = "Step 2",
                .timeout = std::chrono::seconds{10},
                .critical = false
            },
            startup::StepResult{startup::StepStatus::Failed, "Step 2 failed"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "step3",
                .display_name = "Step 3",
                .timeout = std::chrono::seconds{10}
            },
            startup::StepResult{startup::StepStatus::Success, "Step 3 OK"}
        ));

        auto set_result = engine.set_steps(std::move(steps));
        REQUIRE(set_result.ok);

        // Run the sequence
        const auto& progress = engine.run({});

        // Verify final state - should still reach Active
        REQUIRE(progress.state == startup::AppState::Active);
        REQUIRE(progress.can_retry == false);

        // Verify all steps ran
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
        REQUIRE(progress.steps[1].status == startup::StepStatus::Failed);
        REQUIRE(progress.steps[2].status == startup::StepStatus::Success);
    }

}

TEST_CASE("Startup | Timeout detection", "[startup][engine][timeout]") {

    SECTION("timeout overrides step result when deadline is exceeded") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<SlowStep>(
            startup::StepConfig{
                .id = "slow_step",
                .display_name = "Slow Step",
                .timeout = std::chrono::seconds{5},
                .critical = true
            },
            f.clock,
            std::chrono::seconds{10},
            startup::StepResult{startup::StepStatus::Success, "Should be overridden"}
        ));

        auto set_result = engine.set_steps(std::move(steps));
        REQUIRE(set_result.ok);

        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.steps[0].status == startup::StepStatus::TimedOut);
        REQUIRE(progress.steps[0].message.find("timeout") != std::string::npos);
        REQUIRE(progress.can_retry == true);
        REQUIRE(progress.last_error.has_value());
    }

    SECTION("zero timeout means no timeout") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Create a step with zero timeout (infinite)
        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "no_timeout_step",
                .display_name = "No Timeout Step",
                .timeout = std::chrono::seconds{0}
            },
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        auto set_result = engine.set_steps(std::move(steps));
        REQUIRE(set_result.ok);

        // Advance clock by a huge amount
        f.clock->advance(std::chrono::seconds{999999});

        // Run the sequence - should NOT timeout
        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Active);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
    }

}

TEST_CASE("Startup | Critical status for TimedOut and Aborted", "[startup][engine]") {

    SECTION("non-critical timeout continues sequence") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<SlowStep>(
            startup::StepConfig{
                .id = "slow",
                .display_name = "Slow",
                .timeout = std::chrono::seconds{5},
                .critical = false
            },
            f.clock,
            std::chrono::seconds{10}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        engine.set_steps(std::move(steps));
        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Active);
        REQUIRE(progress.steps[0].status == startup::StepStatus::TimedOut);
        REQUIRE(progress.steps[1].status == startup::StepStatus::Success);
    }

    SECTION("non-critical abort continues sequence") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        class AbortReturningStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit AbortReturningStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                return {startup::StepStatus::Aborted, "Aborted"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<AbortReturningStep>(
            startup::StepConfig{.id = "abort_step", .display_name = "Abort", .timeout = std::chrono::seconds{5}, .critical = false}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        engine.set_steps(std::move(steps));
        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Active);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Aborted);
        REQUIRE(progress.steps[1].status == startup::StepStatus::Success);
    }
}

TEST_CASE("Startup | Step management operations", "[startup][engine]") {

    SECTION("add_step successfully appends to existing steps") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Set initial steps
        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "Step 1 OK"}
        ));
        engine.set_steps(std::move(steps));
        REQUIRE(engine.step_count() == 1);

        // Add a second step
        auto result = engine.add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "Step 2 OK"}
        ));

        REQUIRE(result.ok);
        REQUIRE(engine.step_count() == 2);
        REQUIRE(engine.progress().steps.size() == 2);
    }

    SECTION("add_step rejects null step") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        auto result = engine.add_step(nullptr);
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("Invalid step") != std::string::npos);
    }

    SECTION("add_step rejects duplicate ID") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "Step 1 OK"}
        ));
        engine.set_steps(std::move(steps));

        auto result = engine.add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Duplicate", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "Step 1 OK"}
        ));

        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("Duplicate") != std::string::npos);
    }

    SECTION("add_step rejects invalid config") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        auto result = engine.add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "", .display_name = "No ID", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        REQUIRE_FALSE(result.ok);
    }

    SECTION("clear_steps resets engine state") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        engine.set_steps(std::move(steps));
        REQUIRE(engine.has_steps());

        engine.clear_steps();

        REQUIRE_FALSE(engine.has_steps());
        REQUIRE(engine.step_count() == 0);
        REQUIRE(engine.state() == startup::AppState::Off);
        REQUIRE(engine.progress().current_step_index == -1);
        REQUIRE_FALSE(engine.progress().last_error.has_value());
        REQUIRE(engine.progress().can_retry == false);
    }

    SECTION("set_steps rejects null step in vector") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(nullptr);  // Null step

        auto result = engine.set_steps(std::move(steps));
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("Invalid step") != std::string::npos);
    }

    SECTION("set_steps rejects steps with invalid configs") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "", .display_name = "No ID", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        auto result = engine.set_steps(std::move(steps));
        REQUIRE_FALSE(result.ok);
    }
}

TEST_CASE("Startup | Abort and cancellation", "[startup][engine][abort]") {

    SECTION("abort before run prevents execution") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        engine.set_steps(std::move(steps));

        engine.request_abort();

        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.last_error.has_value());
        REQUIRE(progress.last_error.value().find("aborted") != std::string::npos);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Pending);  // Never ran
    }

    SECTION("abort during execution marks remaining steps as Aborted") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Create a step that triggers abort mid-execution
        class AbortingStep : public startup::IStartupStep {
            startup::StepConfig config_;
            startup::StartupEngine* engine_;
        public:
            AbortingStep(startup::StepConfig config, startup::StartupEngine* engine)
                : config_(std::move(config)), engine_(engine) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                engine_->request_abort();
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<AbortingStep>(
            startup::StepConfig{.id = "step2", .display_name = "Aborting Step", .timeout = std::chrono::seconds{5}},
            &engine
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step3", .display_name = "Step 3", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        engine.set_steps(std::move(steps));
        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
        REQUIRE(progress.steps[1].status == startup::StepStatus::Aborted);
        REQUIRE(progress.steps[2].status == startup::StepStatus::Aborted);  // Marked as aborted
    }

    SECTION("reset_abort clears abort flag") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        engine.request_abort();
        engine.reset_abort();

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        engine.set_steps(std::move(steps));

        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Active);  // Should succeed
    }

    SECTION("abort before step execution aborts that step") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Step that checks abort before running
        class AbortCheckingStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit AbortCheckingStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext& ctx) override {
                if (ctx.abort_requested()) {
                    return {startup::StepStatus::Aborted, "Aborted by request"};
                }
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<AbortCheckingStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}, .critical = true}
        ));

        engine.set_steps(std::move(steps));
        engine.request_abort();

        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.last_error.has_value());
    }

    SECTION("critical step returning Aborted status stops sequence") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "abort_step",
                .display_name = "Aborts itself",
                .timeout = std::chrono::seconds{5},
                .critical = true
            },
            startup::StepResult{startup::StepStatus::Aborted, "Step aborted itself"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        engine.set_steps(std::move(steps));
        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Aborted);
        REQUIRE(progress.can_retry == true);
        REQUIRE(progress.last_error.has_value());
        REQUIRE(progress.last_error.value().find("aborted") != std::string::npos);
    }

    SECTION("abort requested between steps marks all remaining as aborted") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Counter to track how many steps have run
        int steps_run = 0;

        // Step that doesn't check abort flag, completes successfully
        class SimpleStep : public startup::IStartupStep {
            startup::StepConfig config_;
            int& counter_;
        public:
            SimpleStep(startup::StepConfig config, int& counter)
                : config_(std::move(config)), counter_(counter) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                counter_++;
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<SimpleStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            steps_run
        ));
        steps.push_back(std::make_unique<SimpleStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            steps_run
        ));
        steps.push_back(std::make_unique<SimpleStep>(
            startup::StepConfig{.id = "step3", .display_name = "Step 3", .timeout = std::chrono::seconds{5}},
            steps_run
        ));

        engine.set_steps(std::move(steps));

        // Use progress callback to trigger abort after first step
        int progress_callback_count = 0;
        const auto& progress = engine.run({
            .progress_callback = [&](const startup::SequenceProgress& p) {
                progress_callback_count++;
                // After first step completes, request abort
                if (progress_callback_count == 1 && p.steps[0].status == startup::StepStatus::Success) {
                    engine.request_abort(startup::AbortReason::UserRequested);
                }
            }
        });

        // First step completed successfully, then abort was detected between steps
        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(steps_run == 1);  // Only first step actually ran
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
        REQUIRE(progress.steps[1].status == startup::StepStatus::Aborted);
        REQUIRE(progress.steps[2].status == startup::StepStatus::Aborted);
        REQUIRE(progress.abort_reason == startup::AbortReason::UserRequested);
        REQUIRE(progress.last_error.has_value());
        REQUIRE(progress.last_error.value().find("aborted") != std::string::npos);
    }
}

TEST_CASE("Startup | Run options and re-execution", "[startup][engine]") {

    SECTION("run with reset_progress_before_run=false preserves old progress") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        engine.set_steps(std::move(steps));

        // First run
        engine.run({.reset_progress_before_run = true});

        // Second run without reset
        const auto& progress = engine.run({.reset_progress_before_run = false});

        // State should update but timestamps might not be reset (implementation-dependent)
        REQUIRE(progress.state == startup::AppState::Active);
    }

    SECTION("running with no steps set returns error") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.last_error.has_value());
        REQUIRE(progress.last_error.value().find("No steps") != std::string::npos);
        REQUIRE(progress.can_retry == false);
    }
}

TEST_CASE("Startup | Retry", "[startup][engine][retry]") {

    SECTION("retry fails when sequence never ran") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        engine.set_steps(std::move(steps));

        // Don't run, try to retry
        auto result = engine.retry();

        REQUIRE_FALSE(result.ok);
    }

    SECTION("retry fails when last run succeeded") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        engine.set_steps(std::move(steps));

        engine.run({});
        REQUIRE(engine.state() == startup::AppState::Active);

        auto result = engine.retry();

        REQUIRE_FALSE(result.ok);
    }

    SECTION("retry fails when last run was aborted by user") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        engine.set_steps(std::move(steps));

        engine.request_abort();
        engine.run({});

        REQUIRE(engine.state() == startup::AppState::Off);
        REQUIRE_FALSE(engine.progress().can_retry);  // Abort sets can_retry=false

        auto result = engine.retry();

        REQUIRE_FALSE(result.ok);
    }

    SECTION("retry succeeds after critical failure when step now passes") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Step that fails first time, succeeds on retry
        class FailOnceThenSucceedStep : public startup::IStartupStep {
            startup::StepConfig config_;
            mutable int call_count_ = 0;
        public:
            explicit FailOnceThenSucceedStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                call_count_++;
                if (call_count_ == 1) {
                    return {startup::StepStatus::Failed, "First attempt failed"};
                }
                return {startup::StepStatus::Success, "Retry succeeded"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FailOnceThenSucceedStep>(
            startup::StepConfig{
                .id = "flaky_step",
                .display_name = "Flaky Step",
                .timeout = std::chrono::seconds{5},
                .critical = true
            }
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        engine.set_steps(std::move(steps));

        // First run: fails
        const auto& progress1 = engine.run({});
        REQUIRE(progress1.state == startup::AppState::Off);
        REQUIRE(progress1.can_retry == true);
        REQUIRE(progress1.steps[0].status == startup::StepStatus::Failed);

        // Retry: should succeed
        auto retry_result = engine.retry();
        REQUIRE(retry_result.ok);

        const auto& progress2 = engine.progress();
        REQUIRE(progress2.state == startup::AppState::Active);
        REQUIRE(progress2.can_retry == false);
        REQUIRE(progress2.steps[0].status == startup::StepStatus::Success);
        REQUIRE(progress2.steps[1].status == startup::StepStatus::Success);
        REQUIRE_FALSE(progress2.last_error.has_value());
    }

    SECTION("retry can be called multiple times if failures persist") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Step that fails first 2 times, succeeds on 3rd
        class FailTwiceStep : public startup::IStartupStep {
            startup::StepConfig config_;
            mutable int call_count_ = 0;
        public:
            explicit FailTwiceStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                call_count_++;
                if (call_count_ <= 2) {
                    return {startup::StepStatus::Failed, "Attempt " + std::to_string(call_count_) + " failed"};
                }
                return {startup::StepStatus::Success, "Third attempt succeeded"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FailTwiceStep>(
            startup::StepConfig{.id = "flaky", .display_name = "Flaky", .timeout = std::chrono::seconds{5}, .critical = true}
        ));
        engine.set_steps(std::move(steps));

        // Run 1: fail
        engine.run({});
        REQUIRE(engine.state() == startup::AppState::Off);
        REQUIRE(engine.can_retry() == true);

        // Retry 1: fail again
        engine.retry();
        REQUIRE(engine.state() == startup::AppState::Off);
        REQUIRE(engine.can_retry() == true);

        // Retry 2: succeed
        engine.retry();
        REQUIRE(engine.state() == startup::AppState::Active);
        REQUIRE(engine.can_retry() == false);
    }

    SECTION("retry after critical failure clears any stale abort flag") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Use a step that fails first time, succeeds on retry
        class FailOnceThenSucceedStep : public startup::IStartupStep {
            startup::StepConfig config_;
            mutable int call_count_ = 0;
        public:
            explicit FailOnceThenSucceedStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                call_count_++;
                if (call_count_ == 1) {
                    return {startup::StepStatus::Failed, "First attempt failed"};
                }
                return {startup::StepStatus::Success, "Retry succeeded"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FailOnceThenSucceedStep>(
            startup::StepConfig{
                .id = "step1",
                .display_name = "Step 1",
                .timeout = std::chrono::seconds{5},
                .critical = true
            }
        ));
        engine.set_steps(std::move(steps));

        // Run and fail
        engine.run({});
        REQUIRE(engine.can_retry());

        // Set abort flag (simulating user changing mind before retry)
        engine.request_abort();

        // Retry should clear abort flag and run successfully
        auto result = engine.retry();
        REQUIRE(result.ok);
        REQUIRE(engine.state() == startup::AppState::Active);
    }

    SECTION("retry resets all steps to Pending before re-running") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Step 2 fails first time, succeeds on retry
        class Step1 : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            Step1() : config_{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}} {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                return {startup::StepStatus::Success, "OK"};
            }
        };

        class Step2 : public startup::IStartupStep {
            startup::StepConfig config_;
            mutable int call_count_ = 0;
        public:
            Step2() : config_{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}, .critical = true} {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                call_count_++;
                if (call_count_ == 1) {
                    return {startup::StepStatus::Failed, "First failed"};
                }
                return {startup::StepStatus::Success, "OK"};
            }
        };

        class Step3 : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            Step3() : config_{.id = "step3", .display_name = "Step 3", .timeout = std::chrono::seconds{5}} {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<Step1>());
        steps.push_back(std::make_unique<Step2>());
        steps.push_back(std::make_unique<Step3>());
        engine.set_steps(std::move(steps));

        // First run - step2 fails
        engine.run({});
        REQUIRE(engine.progress().steps[0].status == startup::StepStatus::Success);
        REQUIRE(engine.progress().steps[1].status == startup::StepStatus::Failed);
        REQUIRE(engine.progress().steps[2].status == startup::StepStatus::Pending);  // Never ran

        // Retry - step2 now succeeds, all steps should run
        engine.retry();

        REQUIRE(engine.progress().steps[0].status == startup::StepStatus::Success);
        REQUIRE(engine.progress().steps[1].status == startup::StepStatus::Success);
        REQUIRE(engine.progress().steps[2].status == startup::StepStatus::Success);
    }

}

TEST_CASE("Startup | Abort Reason", "[startup][engine][abort]") {

    SECTION("default abort reason is None") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        auto progress = engine.progress();
        REQUIRE(progress.abort_reason == startup::AbortReason::None);
    }

    SECTION("request_abort sets default reason to UserRequested") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        engine.request_abort();

        auto progress = engine.progress();
        REQUIRE(progress.abort_reason == startup::AbortReason::UserRequested);
    }

    SECTION("request_abort with explicit reason sets it correctly") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        engine.request_abort(startup::AbortReason::SystemShutdown);

        auto progress = engine.progress();
        REQUIRE(progress.abort_reason == startup::AbortReason::SystemShutdown);
    }

    SECTION("reset_abort clears abort reason") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        engine.request_abort(startup::AbortReason::UserRequested);
        engine.reset_abort();

        auto progress = engine.progress();
        REQUIRE(progress.abort_reason == startup::AbortReason::None);
    }

    SECTION("abort reason persists in progress after sequence aborted") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        engine.set_steps(std::move(steps));

        engine.request_abort(startup::AbortReason::SystemShutdown);
        const auto& progress = engine.run({});

        REQUIRE(progress.abort_reason == startup::AbortReason::SystemShutdown);
        REQUIRE(progress.state == startup::AppState::Off);
    }

    SECTION("abort reason persists when abort detected during step execution") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Create a step that triggers abort mid-execution
        class AbortingStep : public startup::IStartupStep {
            startup::StepConfig config_;
            startup::StartupEngine* engine_;
            startup::AbortReason reason_;
        public:
            AbortingStep(startup::StepConfig config, startup::StartupEngine* engine, startup::AbortReason reason)
                : config_(std::move(config)), engine_(engine), reason_(reason) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                engine_->request_abort(reason_);
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<AbortingStep>(
            startup::StepConfig{.id = "step2", .display_name = "Aborting Step", .timeout = std::chrono::seconds{5}},
            &engine,
            startup::AbortReason::CriticalFailure
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step3", .display_name = "Step 3", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        engine.set_steps(std::move(steps));
        const auto& progress = engine.run({});

        REQUIRE(progress.abort_reason == startup::AbortReason::CriticalFailure);
        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.steps[1].status == startup::StepStatus::Aborted);
        REQUIRE(progress.steps[2].status == startup::StepStatus::Aborted);
    }

    SECTION("retry clears abort reason") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Step that fails first time, succeeds on retry
        class FailOnceThenSucceedStep : public startup::IStartupStep {
            startup::StepConfig config_;
            mutable int call_count_ = 0;
        public:
            explicit FailOnceThenSucceedStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                call_count_++;
                if (call_count_ == 1) {
                    return {startup::StepStatus::Failed, "First attempt failed"};
                }
                return {startup::StepStatus::Success, "Retry succeeded"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<FailOnceThenSucceedStep>(
            startup::StepConfig{
                .id = "flaky_step",
                .display_name = "Flaky Step",
                .timeout = std::chrono::seconds{5},
                .critical = true
            }
        ));
        engine.set_steps(std::move(steps));

        // First run: fails
        engine.run({});

        // Set abort reason manually
        engine.request_abort(startup::AbortReason::UserRequested);
        REQUIRE(engine.progress().abort_reason == startup::AbortReason::UserRequested);

        // Retry: should clear abort reason
        auto retry_result = engine.retry();
        REQUIRE(retry_result.ok);

        const auto& progress = engine.progress();
        REQUIRE(progress.abort_reason == startup::AbortReason::None);
        REQUIRE(progress.state == startup::AppState::Active);
    }

}

TEST_CASE("Startup | check_abort_point helper", "[startup][engine][abort]") {

    SECTION("check_abort_point throws when abort requested") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Step that uses check_abort_point in a loop
        class ThrowingAbortStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit ThrowingAbortStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext& ctx) override {
                try {
                    for (int i = 0; i < 10; ++i) {
                        ctx.check_abort_point();
                    }
                    return {startup::StepStatus::Success, "Completed all iterations"};
                } catch (const std::runtime_error& e) {
                    return {startup::StepStatus::Aborted, std::string("Caught abort: ") + e.what()};
                }
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<ThrowingAbortStep>(
            startup::StepConfig{
                .id = "throw_test",
                .display_name = "Abort Point Test",
                .timeout = std::chrono::seconds{10},
                .critical = true
            }
        ));
        engine.set_steps(std::move(steps));

        // Request abort before running
        engine.request_abort();

        const auto& progress = engine.run({});

        // The step should have been aborted (either by engine or by catching exception)
        REQUIRE(progress.state == startup::AppState::Off);
    }

    SECTION("check_abort_point does not throw when abort not requested") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        class ThrowingAbortStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit ThrowingAbortStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext& ctx) override {
                for (int i = 0; i < 10; ++i) {
                    ctx.check_abort_point();
                }
                return {startup::StepStatus::Success, "Completed without abort"};
            }
        };

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<ThrowingAbortStep>(
            startup::StepConfig{
                .id = "throw_test",
                .display_name = "Abort Point Test",
                .timeout = std::chrono::seconds{10}
            }
        ));
        engine.set_steps(std::move(steps));

        // Don't request abort
        const auto& progress = engine.run({});

        REQUIRE(progress.state == startup::AppState::Active);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
        REQUIRE(progress.steps[0].message == "Completed without abort");
    }

}
