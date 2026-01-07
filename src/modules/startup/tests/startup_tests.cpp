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

// A step that basically does nothing
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