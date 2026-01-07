//
// Created by Nicolas Désilles on 06/01/2026.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <chrono>
#include <vector>

#include <logger.h>
#include <settings.h>

#include "startup.h"
#include "clock.h"
#include "startup_step.h"
#include "startup_engine.h"

using namespace aknet;

namespace fs = std::filesystem;

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

// Helper to create a temporary test directory
class TempDir {
    fs::path path_;
public:
    // Constructor
    TempDir() : path_(fs::temp_directory_path() / "aknet_startup_tests") {
        fs::create_directories(path_);
    }
    // Destructor
    ~TempDir() {
        try {
            fs::remove_all(path_);
        } catch (...) {
            // Prevent crash during cleanup if OS locks files
        }
    }
    const fs::path& path() const { return path_; }
};

// To avoid using sleep()
struct FakeClock : startup::IClock {
    std::chrono::steady_clock::time_point t{};

    std::chrono::steady_clock::time_point now() const override {
        return t;
    }

    void advance(std::chrono::seconds d) {
        t += d;
    }
};

// A step that basically does nothing and returns a pre-defined result
class FakeStep : public startup::IStartupStep {
    startup::StepConfig config_;
    startup::StepResult result_;

public:
    FakeStep(startup::StepConfig cfg, startup::StepResult res)
        : config_(std::move(cfg)), result_(std::move(res)) {}

    const startup::StepConfig& config() const override {
        return config_;
    }

    startup::StepResult run(startup::StepContext& ctx) override {
        return result_;
    }
};

// A step that simulates slow execution by advancing the clock
class SlowStep : public startup::IStartupStep {
    startup::StepConfig config_;
    std::shared_ptr<FakeClock> clock_;
    std::chrono::seconds execution_time_;
    startup::StepResult result_;

public:
    SlowStep(startup::StepConfig cfg,
             std::shared_ptr<FakeClock> clk,
             std::chrono::seconds exec_time,
             startup::StepResult res = {startup::StepStatus::Success, "OK"})
        : config_(std::move(cfg)),
          clock_(std::move(clk)),
          execution_time_(exec_time),
          result_(std::move(res)) {}

    const startup::StepConfig& config() const override {
        return config_;
    }

    startup::StepResult run(startup::StepContext& ctx) override {
        clock_->advance(execution_time_);
        return result_;
    }
};

struct EngineTestFixture {
    std::shared_ptr<log::Logger> logger_settings;
    std::shared_ptr<log::Logger> logger_startup;
    std::shared_ptr<settings::Settings> settings;
    std::shared_ptr<FakeClock> clock;

    EngineTestFixture() {
        TempDir temp_dir;
        log::init(temp_dir.path());
        logger_settings = log::get("settings");
        logger_startup = log::get("startup_t");

        settings = std::make_shared<settings::Settings>();
        settings->init(logger_settings, settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .schema_version = 1
        });

        clock = std::make_shared<FakeClock>();
    }

    ~EngineTestFixture() {
        settings->shutdown();
        log::shutdown();
    }
};

// ------------------------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Startup | Data validation", "[startup]") {

    SECTION("step config with empty id fails validation") {

        startup::StepConfig step_config{
        .display_name = "Test Display Name",
        .timeout = std::chrono::seconds{10}};

        auto validation_result = startup::validate_step_config(step_config);

        REQUIRE_FALSE(validation_result.ok);

    }

    SECTION("step config with empty display name fails validation") {

        startup::StepConfig step_config{
            .id = "test_id",
            .timeout = std::chrono::seconds{10}};

        auto validation_result = startup::validate_step_config(step_config);

        REQUIRE_FALSE(validation_result.ok);

    }

    SECTION("step config with a negative timeout value fails validation") {

        startup::StepConfig step_config{
            .id = "test_id",
            .display_name = "Test Display Name",
            .timeout = std::chrono::seconds{-1}};

        auto validation_result = startup::validate_step_config(step_config);

        REQUIRE_FALSE(validation_result.ok);

    }

    SECTION("valid step config passes validation") {

        startup::StepConfig step_config{
            .id = "test_id",
            .display_name = "Test Display Name",
            .timeout = std::chrono::seconds{10}};

        auto validation_result = startup::validate_step_config(step_config);

        REQUIRE(validation_result.ok);

    }

    SECTION("duplicate step config ids fail validation") {

        std::vector<startup::StepConfig> step_configs;

        startup::StepConfig step_config_1{
            .id = "test_id",
            .display_name = "Test Display Name 1",
            .timeout = std::chrono::seconds{10}};

        startup::StepConfig step_config_2{
            .id = "test_id",
            .display_name = "Test Display Name 2",
            .timeout = std::chrono::seconds{10}};

        step_configs.push_back(step_config_1);
        step_configs.push_back(step_config_2);

        auto validation_result = startup::validate_step_configs_unique(step_configs);

        REQUIRE_FALSE(validation_result.ok);

    }

    SECTION("unique step config ids pass validation") {

        std::vector<startup::StepConfig> step_configs;

        startup::StepConfig step_config_1{
            .id = "test_id_1",
            .display_name = "Test Display Name 1",
            .timeout = std::chrono::seconds{10}};

        startup::StepConfig step_config_2{
            .id = "test_id_2",
            .display_name = "Test Display Name 2",
            .timeout = std::chrono::seconds{10}};

        step_configs.push_back(step_config_1);
        step_configs.push_back(step_config_2);

        auto validation_result = startup::validate_step_configs_unique(step_configs);

        REQUIRE(validation_result.ok);

    }

}

TEST_CASE("Startup | Data validation edge cases", "[startup]") {

    SECTION("validate_step_configs_unique passes with empty vector") {
        std::vector<startup::StepConfig> empty_configs;
        auto result = startup::validate_step_configs_unique(empty_configs);
        REQUIRE(result.ok);
    }

    SECTION("validate_step_configs_unique passes with single config") {
        std::vector<startup::StepConfig> single_config;
        single_config.push_back(startup::StepConfig{
            .id = "single",
            .display_name = "Single",
            .timeout = std::chrono::seconds{5}
        });
        auto result = startup::validate_step_configs_unique(single_config);
        REQUIRE(result.ok);
    }

    SECTION("step config with zero timeout passes validation") {
        startup::StepConfig step_config{
            .id = "test_id",
            .display_name = "Test Display Name",
            .timeout = std::chrono::seconds{0}
        };

        auto validation_result = startup::validate_step_config(step_config);
        REQUIRE(validation_result.ok);
    }
}

TEST_CASE("Startup | Helper functions", "[startup]") {

    SECTION("make_initial_step_progress copies config fields correctly") {
        startup::StepConfig config{
            .id = "test_step",
            .display_name = "Test Step Name",
            .timeout = std::chrono::seconds{42}
        };

        auto progress = startup::make_initial_step_progress(config);

        REQUIRE(progress.id == "test_step");
        REQUIRE(progress.display_name == "Test Step Name");
        REQUIRE(progress.status == startup::StepStatus::Pending);
        REQUIRE(progress.message.empty());
        REQUIRE_FALSE(progress.start_time.has_value());
        REQUIRE_FALSE(progress.end_time.has_value());
    }

    SECTION("make_initial_sequence_progress sets state correctly") {
        std::vector<startup::StepConfig> configs;
        configs.push_back(startup::StepConfig{.id = "s1", .display_name = "S1", .timeout = std::chrono::seconds{5}});

        auto progress = startup::make_initial_sequence_progress(startup::AppState::Booting, configs);

        REQUIRE(progress.state == startup::AppState::Booting);
        REQUIRE(progress.steps.size() == 1);
    }

    SECTION("make_initial_sequence_progress with empty configs") {
        std::vector<startup::StepConfig> empty;
        auto progress = startup::make_initial_sequence_progress(startup::AppState::Off, empty);

        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.steps.empty());
    }
}

TEST_CASE("Startup | Default helpers", "[startup]") {

    SECTION("default status for a step progress is PENDING") {

        startup::StepConfig step_config{
            .id = "test_id",
            .display_name = "Test Display Name",
            .timeout = std::chrono::seconds{10}};

        startup::StepProgress step_progress = make_initial_step_progress(step_config);

        REQUIRE(step_progress.status == startup::StepStatus::Pending);

    }

    SECTION("default sequence progress created with n configs has n steps") {

        std::vector<startup::StepConfig> step_configs;

        startup::StepConfig step_config_1{
            .id = "test_id_1",
            .display_name = "Test Display Name 1",
            .timeout = std::chrono::seconds{10}};

        startup::StepConfig step_config_2{
            .id = "test_id_2",
            .display_name = "Test Display Name 2",
            .timeout = std::chrono::seconds{10}};

        startup::StepConfig step_config_3{
            .id = "test_id_3",
            .display_name = "Test Display Name 3",
            .timeout = std::chrono::seconds{10}};

        step_configs.push_back(step_config_1);
        step_configs.push_back(step_config_2);
        step_configs.push_back(step_config_3);

        startup::SequenceProgress sequence_progress = make_initial_sequence_progress(startup::AppState::Off,step_configs);

        REQUIRE(sequence_progress.steps.size() == step_configs.size());

    }

}

TEST_CASE("Startup | Startup Engine Constructor", "[startup]") {

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

TEST_CASE("Startup | Step registration and validation", "[startup]") {

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
                .id = "duplicate_id",  // Same ID
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

TEST_CASE("Startup | Sequence execution", "[startup]") {

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
                .critical = true  // Critical step
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
                .critical = false  // Non-critical
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

TEST_CASE("Startup | Timeout detection", "[startup]") {

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
            std::chrono::seconds{10},  // Takes 10s, but timeout is 5s
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
                .timeout = std::chrono::seconds{0}  // No timeout
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

TEST_CASE("Startup | Critical status for TimedOut and Aborted", "[startup]") {

    SECTION("non-critical timeout continues sequence") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        std::vector<startup::StartupEngine::StepPtr> steps;
        steps.push_back(std::make_unique<SlowStep>(
            startup::StepConfig{
                .id = "slow",
                .display_name = "Slow",
                .timeout = std::chrono::seconds{5},
                .critical = false  // Non-critical
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
            explicit AbortReturningStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}
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

TEST_CASE("Startup | Step management operations", "[startup]") {

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

TEST_CASE("Startup | Abort and cancellation", "[startup]") {

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
            AbortingStep(startup::StepConfig cfg, startup::StartupEngine* eng)
                : config_(std::move(cfg)), engine_(eng) {}

            const startup::StepConfig& config() const override { return config_; }

            startup::StepResult run(startup::StepContext& ctx) override {
                engine_->request_abort();  // Trigger abort during step
                return {startup::StepStatus::Success, "Aborted internally"};
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
            explicit AbortCheckingStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}
            const startup::StepConfig& config() const override { return config_; }

            startup::StepResult run(startup::StepContext& ctx) override {
                if (ctx.abort_requested()) {
                    return {startup::StepStatus::Aborted, "Detected abort"};
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
            startup::StepResult{startup::StepStatus::Aborted, "Step aborted itself"}  // Returns Aborted
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
}

TEST_CASE("Startup | Run options and re-execution", "[startup]") {

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

TEST_CASE("Startup | Retry", "[startup]") {

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
            explicit FailOnceThenSucceedStep(startup::StepConfig cfg)
                : config_(std::move(cfg)) {}

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
            explicit FailTwiceStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}
            const startup::StepConfig& config() const override { return config_; }

            startup::StepResult run(startup::StepContext&) override {
                call_count_++;
                if (call_count_ <= 2) {
                    return {startup::StepStatus::Failed, "Attempt " + std::to_string(call_count_) + " failed"};
                }
                return {startup::StepStatus::Success, "Finally succeeded"};
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
            explicit FailOnceThenSucceedStep(startup::StepConfig cfg)
                : config_(std::move(cfg)) {}

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
            Step2() : config_{
                .id = "step2",
                .display_name = "Step 2",
                .timeout = std::chrono::seconds{5},
                .critical = true
            } {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                call_count_++;
                if (call_count_ == 1) {
                    return {startup::StepStatus::Failed, "Fail"};
                }
                return {startup::StepStatus::Success, "Now OK"};
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

