//
// Created by Nicolas Désilles on 06/01/2026.
//
// Note for future self: I used some AI to make most of these tests to increase dev speed.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <chrono>
#include <vector>

#include <logger.h>
#include <settings.h>
#include <thread>

#include "startup.h"
#include "clock.h"
#include "startup_step.h"
#include "startup_engine.h"
#include "startup_manager.h"
#include "startup_json.h"

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

TEST_CASE("Startup | Abort Reason", "[startup]") {

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
            AbortingStep(startup::StepConfig cfg, startup::StartupEngine* eng, startup::AbortReason reason)
                : config_(std::move(cfg)), engine_(eng), reason_(reason) {}

            const startup::StepConfig& config() const override { return config_; }

            startup::StepResult run(startup::StepContext& ctx) override {
                engine_->request_abort(reason_);
                return {startup::StepStatus::Success, "Should be overridden"};
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

TEST_CASE("Startup | check_abort_point helper", "[startup]") {

    SECTION("check_abort_point throws when abort requested") {
        EngineTestFixture f;
        startup::StartupEngine engine(f.logger_startup, f.settings, f.clock);

        // Step that uses check_abort_point in a loop
        class ThrowingAbortStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit ThrowingAbortStep(startup::StepConfig cfg)
                : config_(std::move(cfg)) {}

            const startup::StepConfig& config() const override { return config_; }

            startup::StepResult run(startup::StepContext& ctx) override {
                try {
                    for (int i = 0; i < 10; ++i) {
                        ctx.check_abort_point();  // Should throw if aborted
                        // Simulate some work
                    }
                    return {startup::StepStatus::Success, "Completed"};
                } catch (const std::runtime_error& e) {
                    return {startup::StepStatus::Aborted, std::string("Caught: ") + e.what()};
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
            explicit ThrowingAbortStep(startup::StepConfig cfg)
                : config_(std::move(cfg)) {}

            const startup::StepConfig& config() const override { return config_; }

            startup::StepResult run(startup::StepContext& ctx) override {
                try {
                    for (int i = 0; i < 10; ++i) {
                        ctx.check_abort_point();
                    }
                    return {startup::StepStatus::Success, "Completed without abort"};
                } catch (const std::runtime_error&) {
                    return {startup::StepStatus::Aborted, "Should not happen"};
                }
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

TEST_CASE("Startup | StepContext edge cases", "[startup]") {

    SECTION("abort_requested returns false when abort_flag is null") {
        startup::StepContext ctx;
        ctx.abort_flag = nullptr;
        
        REQUIRE_FALSE(ctx.abort_requested());
    }

    SECTION("abort_requested returns true when flag is set") {
        std::atomic_bool abort_flag{true};
        startup::StepContext ctx;
        ctx.abort_flag = &abort_flag;
        
        REQUIRE(ctx.abort_requested());
    }

    SECTION("abort_requested returns false when flag is not set") {
        std::atomic_bool abort_flag{false};
        startup::StepContext ctx;
        ctx.abort_flag = &abort_flag;
        
        REQUIRE_FALSE(ctx.abort_requested());
    }

    SECTION("is_expired returns false when clock is null") {
        startup::StepContext ctx;
        ctx.clock = nullptr;
        
        REQUIRE_FALSE(ctx.is_expired());
    }

    SECTION("is_expired returns true when past deadline") {
        auto clock = std::make_shared<FakeClock>();
        clock->t = std::chrono::steady_clock::now();
        
        startup::StepContext ctx;
        ctx.clock = clock.get();
        ctx.deadline = clock->t - std::chrono::seconds{1};  // Deadline in the past
        
        REQUIRE(ctx.is_expired());
    }

    SECTION("is_expired returns false when before deadline") {
        auto clock = std::make_shared<FakeClock>();
        clock->t = std::chrono::steady_clock::now();
        
        startup::StepContext ctx;
        ctx.clock = clock.get();
        ctx.deadline = clock->t + std::chrono::seconds{10};  // Deadline in the future
        
        REQUIRE_FALSE(ctx.is_expired());
    }

    SECTION("check_abort_point does nothing when abort_flag is null") {
        startup::StepContext ctx;
        ctx.abort_flag = nullptr;
        
        // Should not throw
        REQUIRE_NOTHROW(ctx.check_abort_point());
    }

    SECTION("check_abort_point does nothing when abort not requested") {
        std::atomic_bool abort_flag{false};
        startup::StepContext ctx;
        ctx.abort_flag = &abort_flag;
        
        // Should not throw
        REQUIRE_NOTHROW(ctx.check_abort_point());
    }

    SECTION("check_abort_point throws when abort requested") {
        std::atomic_bool abort_flag{true};
        startup::StepContext ctx;
        ctx.abort_flag = &abort_flag;
        
        // Should throw
        REQUIRE_THROWS_AS(ctx.check_abort_point(), std::runtime_error);
    }

}

TEST_CASE("Startup | StartupManager - Basic async execution", "[startup]") {

    SECTION("start_async runs sequence in background") {
        EngineTestFixture f;
        // Use real clock for async tests
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        auto set_result = manager.set_steps(std::move(steps));
        REQUIRE(set_result.ok);

        auto start_result = manager.start_async();
        REQUIRE(start_result.ok);

        // Wait for completion (poll with timeout)
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        // Also wait a bit more to ensure cache is updated
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE_FALSE(manager.is_running());

        auto progress = manager.get_progress();
        REQUIRE(progress.state == startup::AppState::Active);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
    }

    SECTION("start_async fails when already running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Create a step that takes some time
        class SlowRealStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit SlowRealStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow", .display_name = "Slow", .timeout = std::chrono::seconds{5}}
        ));

        manager.set_steps(std::move(steps));
        auto result1 = manager.start_async();
        REQUIRE(result1.ok);

        // Wait a tiny bit to ensure thread has started
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Try to start again while running
        auto result2 = manager.start_async();
        REQUIRE_FALSE(result2.ok);
        REQUIRE(result2.error.find("already running") != std::string::npos);

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
    }

    SECTION("start_async fails with no steps") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        auto result = manager.start_async();
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("No steps") != std::string::npos);
    }

}

TEST_CASE("Startup | StartupManager - Abort during async execution", "[startup]") {

    SECTION("request_abort stops running sequence") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Create a step that checks abort periodically
        class AbortCheckingStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit AbortCheckingStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}
            const startup::StepConfig& config() const override { return config_; }

            startup::StepResult run(startup::StepContext& ctx) override {
                // Simulate long operation with abort checks
                for (int i = 0; i < 100; ++i) {
                    if (ctx.abort_requested()) {
                        return {startup::StepStatus::Aborted, "Detected abort"};
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                return {startup::StepStatus::Success, "Completed"};
            }
        };

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<AbortCheckingStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{10}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit then abort
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        manager.request_abort(startup::AbortReason::UserRequested);

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 200) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        auto progress = manager.get_progress();
        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.abort_reason == startup::AbortReason::UserRequested);
    }

}

TEST_CASE("Startup | StartupManager - Retry async", "[startup]") {

    SECTION("retry_async re-runs failed sequence") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Step that fails first time, succeeds on retry
        class FailOnceThenSucceedStep : public startup::IStartupStep {
            startup::StepConfig config_;
            mutable std::atomic<int> call_count_{0};
        public:
            explicit FailOnceThenSucceedStep(startup::StepConfig cfg)
                : config_(std::move(cfg)) {}

            const startup::StepConfig& config() const override { return config_; }

            startup::StepResult run(startup::StepContext&) override {
                int count = call_count_.fetch_add(1, std::memory_order_relaxed);
                if (count == 0) {
                    return {startup::StepStatus::Failed, "First attempt failed"};
                }
                return {startup::StepStatus::Success, "Retry succeeded"};
            }
        };

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FailOnceThenSucceedStep>(
            startup::StepConfig{
                .id = "flaky",
                .display_name = "Flaky",
                .timeout = std::chrono::seconds{5},
                .critical = true
            }
        ));

        manager.set_steps(std::move(steps));

        // First run - should fail
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(manager.get_state() == startup::AppState::Off);
        REQUIRE(manager.can_retry());

        // Retry - should succeed
        auto retry_result = manager.retry_async();
        REQUIRE(retry_result.ok);

        attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(manager.get_state() == startup::AppState::Active);
        REQUIRE_FALSE(manager.can_retry());
    }

    SECTION("retry_async fails when not eligible") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));

        // Never ran, can't retry
        auto result = manager.retry_async();
        REQUIRE_FALSE(result.ok);
    }

}

TEST_CASE("Startup | StartupManager - Thread safety", "[startup]") {

    SECTION("get_progress is safe while sequence runs") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Create multiple steps that take a bit of time
        class SlowRealStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit SlowRealStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupManager::StepPtr> steps;
        for (int i = 0; i < 5; ++i) {
            steps.push_back(std::make_unique<SlowRealStep>(
                startup::StepConfig{
                    .id = "step" + std::to_string(i),
                    .display_name = "Step " + std::to_string(i),
                    .timeout = std::chrono::seconds{5}
                }
            ));
        }

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Poll progress rapidly while sequence runs
        int poll_count = 0;
        while (manager.is_running() && poll_count < 50) {
            auto progress = manager.get_progress();  // Should not crash
            REQUIRE(progress.steps.size() == 5);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            poll_count++;
        }

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Final progress after completion
        auto final_progress = manager.get_progress();
        REQUIRE(final_progress.state == startup::AppState::Active);
    }

    SECTION("cannot modify steps while running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Create a step that takes some time
        class SlowRealStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit SlowRealStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit to ensure it's running
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Try to add step while running
        auto result = manager.add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("running") != std::string::npos);

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
    }

}

TEST_CASE("Startup | StartupManager - Lifecycle", "[startup]") {

    SECTION("destructor waits for running sequence") {
        EngineTestFixture f;

        {
            auto real_clock = std::make_shared<startup::SteadyClock>();
            startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

            std::vector<startup::StartupManager::StepPtr> steps;
            steps.push_back(std::make_unique<FakeStep>(
                startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
                startup::StepResult{startup::StepStatus::Success, "OK"}
            ));

            manager.set_steps(std::move(steps));
            manager.start_async();

            // Destructor should wait for completion and clean up
        }

        // If we get here without hanging, destructor worked correctly
        REQUIRE(true);
    }

}

TEST_CASE("Startup | StartupManager - Edge cases", "[startup]") {

    SECTION("set_steps with invalid config returns error and doesn't update cache") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Create steps with duplicate IDs (invalid)
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "duplicate", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "duplicate", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        auto result = manager.set_steps(std::move(steps));

        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("duplicate") != std::string::npos);

        // Progress should still be in Off state with no steps
        auto progress = manager.get_progress();
        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.steps.empty());
    }

    SECTION("add_step with invalid config returns error and doesn't update cache") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Add a valid step first
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        manager.set_steps(std::move(steps));

        // Try to add step with duplicate ID
        auto result = manager.add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Duplicate", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("step1") != std::string::npos);

        // Should still have only 1 step
        auto progress = manager.get_progress();
        REQUIRE(progress.steps.size() == 1);
    }

    SECTION("clear_steps does nothing when sequence is running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Create a step that takes some time
        class SlowRealStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit SlowRealStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return {startup::StepStatus::Success, "OK"};
            }
        };

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow", .display_name = "Slow", .timeout = std::chrono::seconds{5}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit to ensure it's running
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Try to clear steps while running
        manager.clear_steps();

        // Should still have the step (clear was ignored)
        auto progress = manager.get_progress();
        REQUIRE(progress.steps.size() == 1);

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
    }

    SECTION("clear_steps works when not running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));

        // Verify step was added
        REQUIRE(manager.get_progress().steps.size() == 1);

        // Clear steps
        manager.clear_steps();

        // Should have no steps now
        auto progress = manager.get_progress();
        REQUIRE(progress.steps.empty());
    }

    SECTION("get_state returns correct state from cached progress") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Initial state should be Off
        REQUIRE(manager.get_state() == startup::AppState::Off);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Should be Active after successful run
        REQUIRE(manager.get_state() == startup::AppState::Active);
    }

    SECTION("worker thread handles spurious wakeups gracefully") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));

        // Just let the manager sit idle for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should still be idle, nothing should have run
        REQUIRE(manager.get_state() == startup::AppState::Off);
        REQUIRE_FALSE(manager.is_running());

        // Now actually start it
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(manager.get_state() == startup::AppState::Active);
    }

}

TEST_CASE("Startup | JSON - AppState serialization", "[startup]") {
    using namespace startup;

    SECTION("to_json converts AppState to integer") {
        nlohmann::json j;

        to_json(j, AppState::Off);
        REQUIRE(j.is_number_integer());
        REQUIRE(j.get<int>() == 0);

        to_json(j, AppState::Booting);
        REQUIRE(j.get<int>() == 1);

        to_json(j, AppState::Active);
        REQUIRE(j.get<int>() == 2);

        to_json(j, AppState::ShuttingDown);
        REQUIRE(j.get<int>() == 3);
    }

    SECTION("from_json converts integer to AppState") {
        AppState state;

        from_json(nlohmann::json(0), state);
        REQUIRE(state == AppState::Off);

        from_json(nlohmann::json(1), state);
        REQUIRE(state == AppState::Booting);

        from_json(nlohmann::json(2), state);
        REQUIRE(state == AppState::Active);

        from_json(nlohmann::json(3), state);
        REQUIRE(state == AppState::ShuttingDown);
    }

    SECTION("round-trip AppState") {
        for (auto original : {AppState::Off, AppState::Booting, AppState::Active, AppState::ShuttingDown}) {
            nlohmann::json j = original;
            AppState restored = j.get<AppState>();
            REQUIRE(restored == original);
        }
    }

    SECTION("from_json throws on invalid integer") {
        AppState state;
        REQUIRE_THROWS(from_json(nlohmann::json(999), state));
    }
}

TEST_CASE("Startup | JSON - StepStatus serialization", "[startup]") {
    using namespace startup;

    SECTION("to_json converts StepStatus to integer") {
        nlohmann::json j;

        to_json(j, StepStatus::Pending);
        REQUIRE(j.get<int>() == 0);

        to_json(j, StepStatus::Running);
        REQUIRE(j.get<int>() == 1);

        to_json(j, StepStatus::Success);
        REQUIRE(j.get<int>() == 2);

        to_json(j, StepStatus::Failed);
        REQUIRE(j.get<int>() == 3);

        to_json(j, StepStatus::TimedOut);
        REQUIRE(j.get<int>() == 4);

        to_json(j, StepStatus::Skipped);
        REQUIRE(j.get<int>() == 5);

        to_json(j, StepStatus::Aborted);
        REQUIRE(j.get<int>() == 6);
    }

    SECTION("from_json converts integer to StepStatus") {
        StepStatus status;

        from_json(nlohmann::json(0), status);
        REQUIRE(status == StepStatus::Pending);

        from_json(nlohmann::json(6), status);
        REQUIRE(status == StepStatus::Aborted);
    }

    SECTION("round-trip StepStatus") {
        for (auto original : {StepStatus::Pending, StepStatus::Running, StepStatus::Success,
                              StepStatus::Failed, StepStatus::TimedOut, StepStatus::Skipped, StepStatus::Aborted}) {
            nlohmann::json j = original;
            StepStatus restored = j.get<StepStatus>();
            REQUIRE(restored == original);
                              }
    }

    SECTION("from_json throws on invalid integer") {
        StepStatus status;
        REQUIRE_THROWS(from_json(nlohmann::json(999), status));
    }
}

TEST_CASE("Startup | JSON - AbortReason serialization", "[startup]") {
    using namespace startup;

    SECTION("to_json converts AbortReason to integer") {
        nlohmann::json j;

        to_json(j, AbortReason::None);
        REQUIRE(j.get<int>() == 0);

        to_json(j, AbortReason::UserRequested);
        REQUIRE(j.get<int>() == 1);

        to_json(j, AbortReason::Timeout);
        REQUIRE(j.get<int>() == 2);

        to_json(j, AbortReason::CriticalFailure);
        REQUIRE(j.get<int>() == 3);

        to_json(j, AbortReason::SystemShutdown);
        REQUIRE(j.get<int>() == 4);
    }

    SECTION("round-trip AbortReason") {
        for (auto original : {AbortReason::None, AbortReason::UserRequested, AbortReason::Timeout,
                              AbortReason::CriticalFailure, AbortReason::SystemShutdown}) {
            nlohmann::json j = original;
            AbortReason restored = j.get<AbortReason>();
            REQUIRE(restored == original);}
    }

    SECTION("from_json throws on invalid integer") {
        AbortReason reason;
        REQUIRE_THROWS(from_json(nlohmann::json(-1), reason));
        REQUIRE_THROWS(from_json(nlohmann::json(999), reason));
    }
}

TEST_CASE("Startup | JSON - Result serialization", "[startup]") {
    using namespace startup;

    SECTION("to_json converts Result to object") {
        Result result{true, ""};
        nlohmann::json j = result;

        REQUIRE(j.is_object());
        REQUIRE(j["ok"].get<bool>() == true);
        REQUIRE(j["error"].get<std::string>() == "");
    }

    SECTION("to_json includes error message") {
        Result result{false, "Something went wrong"};
        nlohmann::json j = result;

        REQUIRE(j["ok"].get<bool>() == false);
        REQUIRE(j["error"].get<std::string>() == "Something went wrong");
    }

    SECTION("from_json converts object to Result") {
        nlohmann::json j = {{"ok", true}, {"error", ""}};
        Result result = j.get<Result>();

        REQUIRE(result.ok == true);
        REQUIRE(result.error == "");
    }

    SECTION("round-trip Result") {
        Result original{false, "Test error"};
        nlohmann::json j = original;
        Result restored = j.get<Result>();

        REQUIRE(restored.ok == original.ok);
        REQUIRE(restored.error == original.error);
    }
}

