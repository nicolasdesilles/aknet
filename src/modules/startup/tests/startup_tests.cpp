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
#include <memory>

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

// A step that takes real time to execute (for abort testing)
class SlowRealStep : public startup::IStartupStep {
    startup::StepConfig config_;
public:
    explicit SlowRealStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}

    const startup::StepConfig& config() const override { return config_; }

    startup::StepResult run(startup::StepContext& ctx) override {
        // Sleep in small increments to allow abort checking
        for (int i = 0; i < 15; ++i) {
            if (ctx.abort_requested()) {
                return {startup::StepStatus::Aborted, "Aborted"};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return {startup::StepStatus::Success, "Completed"};
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
    TempDir temp_dir;

    EngineTestFixture() {

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
        REQUIRE(j["error"].get<std::string>().empty());
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
        REQUIRE(result.error.empty());
    }

    SECTION("round-trip Result") {
        Result original{false, "Test error"};
        nlohmann::json j = original;
        Result restored = j.get<Result>();

        REQUIRE(restored.ok == original.ok);
        REQUIRE(restored.error == original.error);
    }
}

TEST_CASE("Startup | JSON - StepConfig serialization", "[startup]") {
    using namespace startup;

    SECTION("to_json converts StepConfig to object") {
        StepConfig config{
            .id = "test_step",
            .display_name = "Test Step",
            .timeout = std::chrono::seconds{30},
            .critical = true,
            .can_skip = false
        };

        nlohmann::json j = config;

        REQUIRE(j["id"].get<std::string>() == "test_step");
        REQUIRE(j["display_name"].get<std::string>() == "Test Step");
        REQUIRE(j["timeout"].get<int>() == 30);  // Timeout in seconds
        REQUIRE(j["critical"].get<bool>() == true);
        REQUIRE(j["can_skip"].get<bool>() == false);
    }

    SECTION("from_json converts object to StepConfig") {
        nlohmann::json j = {
            {"id", "my_step"},
            {"display_name", "My Step"},
            {"timeout", 60},
            {"critical", false},
            {"can_skip", true}
        };

        StepConfig config = j.get<StepConfig>();

        REQUIRE(config.id == "my_step");
        REQUIRE(config.display_name == "My Step");
        REQUIRE(config.timeout == std::chrono::seconds{60});
        REQUIRE(config.critical == false);
        REQUIRE(config.can_skip == true);
    }

    SECTION("round-trip StepConfig") {
        StepConfig original{
            .id = "step_1",
            .display_name = "Step One",
            .timeout = std::chrono::seconds{45},
            .critical = true,
            .can_skip = false
        };

        nlohmann::json j = original;
        StepConfig restored = j.get<StepConfig>();

        REQUIRE(restored.id == original.id);
        REQUIRE(restored.display_name == original.display_name);
        REQUIRE(restored.timeout == original.timeout);
        REQUIRE(restored.critical == original.critical);
        REQUIRE(restored.can_skip == original.can_skip);
    }

    SECTION("timeout of zero serializes correctly") {
        StepConfig config{
            .id = "no_timeout",
            .display_name = "No Timeout",
            .timeout = std::chrono::seconds{0}
        };

        nlohmann::json j = config;
        REQUIRE(j["timeout"].get<int>() == 0);

        StepConfig restored = j.get<StepConfig>();
        REQUIRE(restored.timeout == std::chrono::seconds{0});
    }
}

TEST_CASE("Startup | JSON - StepProgress serialization", "[startup]") {
    using namespace startup;

    SECTION("to_json converts StepProgress with no times") {
        StepProgress progress{
            .id = "step1",
            .display_name = "Step 1",
            .status = StepStatus::Pending,
            .message = "Waiting",
            .start_time = std::nullopt,
            .end_time = std::nullopt
        };

        nlohmann::json j = progress;

        REQUIRE(j["id"].get<std::string>() == "step1");
        REQUIRE(j["display_name"].get<std::string>() == "Step 1");
        REQUIRE(j["status"].get<int>() == 0);  // Pending
        REQUIRE(j["message"].get<std::string>() == "Waiting");
        REQUIRE(j["start_time"].is_null());
        REQUIRE(j["end_time"].is_null());
    }

    SECTION("to_json converts StepProgress with start_time") {
        auto now = std::chrono::steady_clock::now();

        StepProgress progress{
            .id = "step2",
            .display_name = "Step 2",
            .status = StepStatus::Running,
            .message = "In progress",
            .start_time = now,
            .end_time = std::nullopt
        };

        nlohmann::json j = progress;

        REQUIRE(j["status"].get<int>() == 1);  // Running
        REQUIRE(j["start_time"].is_number());
        REQUIRE(j["end_time"].is_null());
    }

    SECTION("to_json converts StepProgress with both times") {
        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::milliseconds{1234};

        StepProgress progress{
            .id = "step3",
            .display_name = "Step 3",
            .status = StepStatus::Success,
            .message = "Completed",
            .start_time = start,
            .end_time = end
        };

        nlohmann::json j = progress;

        REQUIRE(j["status"].get<int>() == 2);  // Success
        REQUIRE(j["start_time"].is_number());
        REQUIRE(j["end_time"].is_number());

        // Verify end_time > start_time
        REQUIRE(j["end_time"].get<int64_t>() > j["start_time"].get<int64_t>());
    }

    SECTION("from_json converts object with null times") {
        nlohmann::json j = {
            {"id", "test"},
            {"display_name", "Test"},
            {"status", 0},
            {"message", "msg"},
            {"start_time", nullptr},
            {"end_time", nullptr}
        };

        StepProgress progress = j.get<StepProgress>();

        REQUIRE(progress.id == "test");
        REQUIRE(progress.status == StepStatus::Pending);
        REQUIRE(progress.start_time == std::nullopt);
        REQUIRE(progress.end_time == std::nullopt);
    }

    SECTION("from_json converts object with times") {
        nlohmann::json j = {
            {"id", "test"},
            {"display_name", "Test"},
            {"status", 2},
            {"message", "done"},
            {"start_time", 1000},
            {"end_time", 2500}
        };

        StepProgress progress = j.get<StepProgress>();

        REQUIRE(progress.start_time.has_value());
        REQUIRE(progress.end_time.has_value());
    }

    SECTION("round-trip StepProgress without times") {
        StepProgress original{
            .id = "step_x",
            .display_name = "Step X",
            .status = StepStatus::Failed,
            .message = "Error occurred"
        };

        nlohmann::json j = original;
        StepProgress restored = j.get<StepProgress>();

        REQUIRE(restored.id == original.id);
        REQUIRE(restored.display_name == original.display_name);
        REQUIRE(restored.status == original.status);
        REQUIRE(restored.message == original.message);
        REQUIRE(restored.start_time == std::nullopt);
        REQUIRE(restored.end_time == std::nullopt);
    }

    SECTION("empty message serializes correctly") {
        StepProgress progress{
            .id = "step",
            .display_name = "Step",
            .status = StepStatus::Pending,
            .message = ""
        };

        nlohmann::json j = progress;
        REQUIRE(j["message"].get<std::string>().empty());
    }
}

TEST_CASE("Startup | JSON - SequenceProgress serialization", "[startup]") {
    using namespace startup;

    SECTION("to_json converts empty SequenceProgress") {
        SequenceProgress progress{
            .state = AppState::Off,
            .current_step_index = -1,
            .steps = {},
            .last_error = std::nullopt,
            .can_retry = false,
            .abort_reason = AbortReason::None
        };

        nlohmann::json j = progress;

        REQUIRE(j["state"].get<int>() == 0);  // Off
        REQUIRE(j["current_step_index"].get<int>() == -1);
        REQUIRE(j["steps"].is_array());
        REQUIRE(empty(j["steps"]));
        REQUIRE(j["last_error"].is_null());
        REQUIRE(j["can_retry"].get<bool>() == false);
        REQUIRE(j["abort_reason"].get<int>() == 0);  // None
    }

    SECTION("to_json converts SequenceProgress with steps") {
        SequenceProgress progress;
        progress.state = AppState::Booting;
        progress.current_step_index = 1;
        progress.steps.push_back(StepProgress{
            .id = "step1",
            .display_name = "Step 1",
            .status = StepStatus::Success,
            .message = "Done"
        });
        progress.steps.push_back(StepProgress{
            .id = "step2",
            .display_name = "Step 2",
            .status = StepStatus::Running,
            .message = "In progress"
        });
        progress.last_error = std::nullopt;
        progress.can_retry = false;
        progress.abort_reason = AbortReason::None;

        nlohmann::json j = progress;

        REQUIRE(j["state"].get<int>() == 1);  // Booting
        REQUIRE(j["current_step_index"].get<int>() == 1);
        REQUIRE(j["steps"].is_array());
        REQUIRE(j["steps"].size() == 2);
        REQUIRE(j["steps"][0]["id"].get<std::string>() == "step1");
        REQUIRE(j["steps"][1]["id"].get<std::string>() == "step2");
        REQUIRE(j["last_error"].is_null());
    }

    SECTION("to_json includes last_error when present") {
        SequenceProgress progress;
        progress.state = AppState::Off;
        progress.last_error = "Critical failure in step 3";
        progress.can_retry = true;
        progress.abort_reason = AbortReason::CriticalFailure;

        nlohmann::json j = progress;

        REQUIRE(j["last_error"].get<std::string>() == "Critical failure in step 3");
        REQUIRE(j["can_retry"].get<bool>() == true);
        REQUIRE(j["abort_reason"].get<int>() == 3);  // CriticalFailure
    }

    SECTION("from_json converts object to SequenceProgress") {
        nlohmann::json j = {
            {"state", 2},  // Active
            {"current_step_index", 2},
            {"steps", nlohmann::json::array()},
            {"last_error", nullptr},
            {"can_retry", false},
            {"abort_reason", 0}
        };

        SequenceProgress progress = j.get<SequenceProgress>();

        REQUIRE(progress.state == AppState::Active);
        REQUIRE(progress.current_step_index == 2);
        REQUIRE(empty(progress.steps));
        REQUIRE(progress.last_error == std::nullopt);
        REQUIRE(progress.can_retry == false);
        REQUIRE(progress.abort_reason == AbortReason::None);
    }

    SECTION("from_json converts object with steps array") {
        nlohmann::json j = {
            {"state", 1},
            {"current_step_index", 0},
            {"steps", nlohmann::json::array({
                {
                    {"id", "s1"},
                    {"display_name", "Step 1"},
                    {"status", 2},
                    {"message", "OK"},
                    {"start_time", nullptr},
                    {"end_time", nullptr}
                }
            })},
            {"last_error", nullptr},
            {"can_retry", false},
            {"abort_reason", 0}
        };

        SequenceProgress progress = j.get<SequenceProgress>();

        REQUIRE(progress.steps.size() == 1);
        REQUIRE(progress.steps[0].id == "s1");
        REQUIRE(progress.steps[0].status == StepStatus::Success);
    }

    SECTION("round-trip SequenceProgress with complete data") {
        SequenceProgress original;
        original.state = AppState::Off;
        original.current_step_index = 2;
        original.steps.push_back(StepProgress{
            .id = "step1",
            .display_name = "First Step",
            .status = StepStatus::Success,
            .message = "Completed successfully"
        });
        original.steps.push_back(StepProgress{
            .id = "step2",
            .display_name = "Second Step",
            .status = StepStatus::Failed,
            .message = "Network error"
        });
        original.last_error = "Step 2 failed: Network error";
        original.can_retry = true;
        original.abort_reason = AbortReason::UserRequested;

        nlohmann::json j = original;
        SequenceProgress restored = j.get<SequenceProgress>();

        REQUIRE(restored.state == original.state);
        REQUIRE(restored.current_step_index == original.current_step_index);
        REQUIRE(restored.steps.size() == original.steps.size());
        REQUIRE(restored.steps[0].id == original.steps[0].id);
        REQUIRE(restored.steps[1].status == original.steps[1].status);
        REQUIRE(restored.last_error == original.last_error);
        REQUIRE(restored.can_retry == original.can_retry);
        REQUIRE(restored.abort_reason == original.abort_reason);
    }
}

TEST_CASE("Startup | JSON - High-level serialization helpers", "[startup]") {
    using namespace startup;

    SECTION("serialize_result produces valid JSON string") {
        Result result{true, ""};
        std::string json_str = serialize_result(result);

        REQUIRE_FALSE(json_str.empty());

        // Parse it back to verify it's valid JSON
        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["ok"].get<bool>() == true);
    }

    SECTION("serialize_result handles error message") {
        Result result{false, "Something failed"};
        std::string json_str = serialize_result(result);

        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["ok"].get<bool>() == false);
        REQUIRE(j["error"].get<std::string>() == "Something failed");
    }

    SECTION("serialize_step_config produces valid JSON string") {
        StepConfig config{
            .id = "test",
            .display_name = "Test",
            .timeout = std::chrono::seconds{10},
            .critical = true,
            .can_skip = false
        };

        std::string json_str = serialize_step_config(config);

        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["id"].get<std::string>() == "test");
        REQUIRE(j["timeout"].get<int>() == 10);
    }

    SECTION("serialize_progress produces valid JSON string") {
        SequenceProgress progress;
        progress.state = AppState::Booting;
        progress.current_step_index = 0;
        progress.can_retry = false;
        progress.abort_reason = AbortReason::None;

        std::string json_str = serialize_progress(progress);

        REQUIRE_FALSE(json_str.empty());

        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["state"].get<int>() == 1);
        REQUIRE(j["current_step_index"].get<int>() == 0);
    }

    SECTION("serialize_progress with complex data") {
        SequenceProgress progress;
        progress.state = AppState::Off;
        progress.current_step_index = 1;
        progress.steps.push_back(StepProgress{
            .id = "s1",
            .display_name = "Step 1",
            .status = StepStatus::Success,
            .message = "OK"
        });
        progress.steps.push_back(StepProgress{
            .id = "s2",
            .display_name = "Step 2",
            .status = StepStatus::Failed,
            .message = "Error"
        });
        progress.last_error = "Step 2 failed";
        progress.can_retry = true;
        progress.abort_reason = AbortReason::CriticalFailure;

        std::string json_str = serialize_progress(progress);

        auto j = nlohmann::json::parse(json_str);
        REQUIRE(j["steps"].is_array());
        REQUIRE(j["steps"].size() == 2);
        REQUIRE(j["last_error"].get<std::string>() == "Step 2 failed");
    }
}

TEST_CASE("Startup | JSON - High-level deserialization helpers", "[startup]") {
    using namespace startup;

    SECTION("deserialize_result parses valid JSON") {
        std::string json_str = R"({"ok": true, "error": ""})";
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == true);
        REQUIRE(result.ok == true);
        REQUIRE(result.error.empty());
    }

    SECTION("deserialize_result handles error") {
        std::string json_str = R"({"ok": false, "error": "Failed"})";
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == true);
        REQUIRE(result.ok == false);
        REQUIRE(result.error == "Failed");
    }

    SECTION("deserialize_result returns error on invalid JSON") {
        std::string json_str = "not valid json {{{";
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == false);
        REQUIRE_FALSE(parse_result.error.empty());
    }

    SECTION("deserialize_result returns error on missing fields") {
        std::string json_str = R"({"ok": true})";  // Missing "error" field
        Result result;

        auto parse_result = deserialize_result(json_str, result);

        REQUIRE(parse_result.ok == false);
    }

    SECTION("deserialize_step_config parses valid JSON") {
        std::string json_str = R"({
            "id": "test",
            "display_name": "Test Step",
            "timeout": 30,
            "critical": true,
            "can_skip": false
        })";
        StepConfig config;

        auto parse_result = deserialize_step_config(json_str, config);

        REQUIRE(parse_result.ok == true);
        REQUIRE(config.id == "test");
        REQUIRE(config.display_name == "Test Step");
        REQUIRE(config.timeout == std::chrono::seconds{30});
        REQUIRE(config.critical == true);
        REQUIRE(config.can_skip == false);
    }

    SECTION("deserialize_progress parses valid JSON") {
        std::string json_str = R"({
            "state": 1,
            "current_step_index": 0,
            "steps": [],
            "last_error": null,
            "can_retry": false,
            "abort_reason": 0
        })";
        SequenceProgress progress;

        auto parse_result = deserialize_progress(json_str, progress);

        REQUIRE(parse_result.ok == true);
        REQUIRE(progress.state == AppState::Booting);
        REQUIRE(progress.current_step_index == 0);
        REQUIRE(progress.steps.empty());
    }

    SECTION("deserialize_progress with steps array") {
        std::string json_str = R"({
            "state": 2,
            "current_step_index": 1,
            "steps": [
                {
                    "id": "s1",
                    "display_name": "Step 1",
                    "status": 2,
                    "message": "Done",
                    "start_time": null,
                    "end_time": null
                }
            ],
            "last_error": "Some error",
            "can_retry": true,
            "abort_reason": 1
        })";
        SequenceProgress progress;

        auto parse_result = deserialize_progress(json_str, progress);

        REQUIRE(parse_result.ok == true);
        REQUIRE(progress.state == AppState::Active);
        REQUIRE(progress.steps.size() == 1);
        REQUIRE(progress.steps[0].id == "s1");
        REQUIRE(progress.last_error.has_value());
        REQUIRE(progress.last_error.value() == "Some error");
        REQUIRE(progress.abort_reason == AbortReason::UserRequested);
    }

    SECTION("deserialize_progress returns error on invalid JSON") {
        std::string json_str = "invalid";
        SequenceProgress progress;

        auto parse_result = deserialize_progress(json_str, progress);

        REQUIRE(parse_result.ok == false);
    }

    SECTION("round-trip via serialize/deserialize") {
        SequenceProgress original;
        original.state = AppState::Off;
        original.current_step_index = -1;
        original.can_retry = true;
        original.abort_reason = AbortReason::None;

        std::string json_str = serialize_progress(original);
        SequenceProgress restored;
        auto parse_result = deserialize_progress(json_str, restored);

        REQUIRE(parse_result.ok == true);
        REQUIRE(restored.state == original.state);
        REQUIRE(restored.current_step_index == original.current_step_index);
        REQUIRE(restored.can_retry == original.can_retry);
    }
}

TEST_CASE("Startup | Events - Basic subscription works", "[startup]") {

    // Setup
    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("events() returns valid event manager") {
        auto& events = manager.events();
        REQUIRE_NOTHROW(events.get<startup::StartupManager::Event::ProgressChanged>());
    }

    SECTION("can subscribe to ProgressChanged event") {
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&event_fired](const startup::SequenceProgress&) {
                event_fired = true;
            });

        // Event hasn't fired yet
        REQUIRE_FALSE(event_fired);
    }

    SECTION("can subscribe to StateChanged event") {
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&event_fired](startup::AppState, startup::AppState) {
                event_fired = true;
            });

        REQUIRE_FALSE(event_fired);
    }

    SECTION("can subscribe to multiple events") {
        int progress_count = 0;
        int state_count = 0;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&progress_count](const startup::SequenceProgress&) {
                progress_count++;
            });

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&state_count](startup::AppState, startup::AppState) {
                state_count++;
            });

        REQUIRE(progress_count == 0);
        REQUIRE(state_count == 0);
    }
}

TEST_CASE("Startup | Events - StateChanged fires on state transitions", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("StateChanged fires when sequence starts (off to booting)") {
        std::vector<std::pair<startup::AppState, startup::AppState>> state_changes;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
        .add([&](startup::AppState old_s, startup::AppState new_s) {
            state_changes.push_back({old_s, new_s});
        });

        // Setup a simple successful step
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait for at least one event
        int attempts = 0;
        while (state_changes.empty() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        REQUIRE_FALSE(state_changes.empty());
        REQUIRE(state_changes[0].first == startup::AppState::Off);
        REQUIRE(state_changes[0].second == startup::AppState::Booting);
    }

    SECTION("StateChanged fires when sequence completes (booting to active)") {
        std::vector<std::pair<startup::AppState, startup::AppState>> state_changes;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                state_changes.push_back({old_s, new_s});
            });

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

        // Give events time to fire
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have two state changes: Off to Booting, Booting to Active
        REQUIRE(state_changes.size() >= 2);
        REQUIRE(state_changes[0].first == startup::AppState::Off);
        REQUIRE(state_changes[0].second == startup::AppState::Booting);
        REQUIRE(state_changes[1].first == startup::AppState::Booting);
        REQUIRE(state_changes[1].second == startup::AppState::Active);
    }

    SECTION("StateChanged fires on failure (booting to off)") {
        std::vector<std::pair<startup::AppState, startup::AppState>> state_changes;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                state_changes.push_back({old_s, new_s});
            });

        // Critical step that fails
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}, .critical = true},
            startup::StepResult{startup::StepStatus::Failed, "Error"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have two state changes: Off to Booting, Booting to Off
        REQUIRE(state_changes.size() >= 2);
        REQUIRE(state_changes[0].second == startup::AppState::Booting);
        REQUIRE(state_changes[1].second == startup::AppState::Off);
    }
}

TEST_CASE("Startup | Events - ProgressChanged fires during sequence", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("ProgressChanged fires at least once during sequence") {
        int progress_count = 0;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress&) {
                progress_count++;
            });

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

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should fire at least once (at completion)
        REQUIRE(progress_count >= 1);
    }

    SECTION("ProgressChanged receives correct progress snapshot") {
        startup::SequenceProgress received_progress;
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress& progress) {
                received_progress = progress;
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "my_step", .display_name = "My Step", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "Done"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(received_progress.state == startup::AppState::Active);
        REQUIRE(received_progress.steps.size() == 1);
        REQUIRE(received_progress.steps[0].id == "my_step");
        REQUIRE(received_progress.steps[0].status == startup::StepStatus::Success);
    }

    SECTION("ProgressChanged fires multiple times for multiple steps") {
        int progress_count = 0;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress&) {
                progress_count++;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step3", .display_name = "Step 3", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should fire at least 3 times (once after each step, minimum)
        REQUIRE(progress_count >= 3);
    }
}

TEST_CASE("Startup | Events - StepStarted and StepCompleted fire", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("StepStarted fires when step begins") {
        int step_index = -1;
        std::string step_id;
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int index, const std::string& id) {
                step_index = index;
                step_id = id;
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "test_step", .display_name = "Test", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(step_index == 0);
        REQUIRE(step_id == "test_step");
    }

    SECTION("StepCompleted fires when step finishes") {
        int step_index = -1;
        std::string step_id;
        startup::StepStatus status = startup::StepStatus::Pending;
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int index, const std::string& id, startup::StepStatus st) {
                step_index = index;
                step_id = id;
                status = st;
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "completed_step", .display_name = "Test", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "Done"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(step_index == 0);
        REQUIRE(step_id == "completed_step");
        REQUIRE(status == startup::StepStatus::Success);
    }

    SECTION("Both events fire in correct order for multiple steps") {
        std::vector<std::string> event_log;

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int index, const std::string& id) {
                event_log.push_back("START:" + id);
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int index, const std::string& id, startup::StepStatus) {
                event_log.push_back("COMPLETE:" + id);
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_log.size() == 4);
        REQUIRE(event_log[0] == "START:step1");
        REQUIRE(event_log[1] == "COMPLETE:step1");
        REQUIRE(event_log[2] == "START:step2");
        REQUIRE(event_log[3] == "COMPLETE:step2");
    }

    SECTION("StepCompleted reports Failed status correctly") {
        startup::StepStatus final_status = startup::StepStatus::Pending;

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int, const std::string&, startup::StepStatus st) {
                final_status = st;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "fail_step", .display_name = "Fail", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Failed, "Error"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(final_status == startup::StepStatus::Failed);
    }
}

TEST_CASE("Startup | Events - Error event fires on critical failures", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("error event fires when critical step fails") {
        std::string error_message;
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::Error>()
            .add([&](const std::string& msg) {
                error_message = msg;
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "critical_step", .display_name = "Critical", .timeout = std::chrono::seconds{5}, .critical = true},
            startup::StepResult{startup::StepStatus::Failed, "Database connection failed"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE_FALSE(error_message.empty());
        REQUIRE(error_message.find("critical_step") != std::string::npos);
    }

    SECTION("error event does not fire on non-critical failure") {
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::Error>()
            .add([&](const std::string&) {
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "non_critical", .display_name = "Non-Critical", .timeout = std::chrono::seconds{5}, .critical = false},
            startup::StepResult{startup::StepStatus::Failed, "Optional service unavailable"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "next_step", .display_name = "Next", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should NOT fire error for non-critical failure
        REQUIRE_FALSE(event_fired);
    }
}

TEST_CASE("Startup | Events - Complete event flow integration", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("all events fire in correct order for successful sequence") {
        std::vector<std::string> event_log;

        // Subscribe to all events
        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                event_log.push_back("STATE:" + std::to_string(static_cast<int>(old_s)) + "->" + std::to_string(static_cast<int>(new_s)));
            });

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int index, const std::string& id) {
                event_log.push_back("STEP_START:" + std::to_string(index) + ":" + id);
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int index, const std::string& id, startup::StepStatus) {
                event_log.push_back("STEP_DONE:" + std::to_string(index) + ":" + id);
            });

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress&) {
                event_log.push_back("PROGRESS");
            });

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool success, const std::string&) {
                event_log.push_back("SEQUENCE:" + std::string(success ? "SUCCESS" : "FAIL"));
            });

        // Setup two-step sequence
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "s1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "s2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_log.size() >= 7);

        // First event should be state change Off to Booting
        REQUIRE(event_log[0] == "STATE:0->1");

        // Should have step events for both steps
        bool found_s1_start = false;
        bool found_s1_done = false;
        bool found_s2_start = false;
        bool found_s2_done = false;

        for (const auto& evt : event_log) {
            if (evt == "STEP_START:0:s1") found_s1_start = true;
            if (evt == "STEP_DONE:0:s1") found_s1_done = true;
            if (evt == "STEP_START:1:s2") found_s2_start = true;
            if (evt == "STEP_DONE:1:s2") found_s2_done = true;
        }

        REQUIRE(found_s1_start);
        REQUIRE(found_s1_done);
        REQUIRE(found_s2_start);
        REQUIRE(found_s2_done);

        // Last event should be SequenceCompleted with success
        REQUIRE(event_log.back() == "SEQUENCE:SUCCESS");
    }

    SECTION("Events fire in deterministic order") {
        std::vector<std::string> event_order;
        std::mutex log_mutex;  // Protect shared vector from race conditions

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("STATE");
            });

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int, const std::string&) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("STEP_START");
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int, const std::string&, startup::StepStatus) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("STEP_COMPLETE");
            });

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress&) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("PROGRESS");
            });

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool, const std::string&) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("SEQUENCE");
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Verify order: STATE (Off→Booting), STEP_START, STEP_COMPLETE, PROGRESS, STATE (Booting→Active), SEQUENCE
        REQUIRE(event_order.size() >= 6);
        REQUIRE(event_order[0] == "STATE");  // Off→Booting
        REQUIRE(event_order[1] == "STEP_START");
        REQUIRE(event_order[2] == "STEP_COMPLETE");
        REQUIRE(event_order.back() == "SEQUENCE");
    }
}

TEST_CASE("Startup | Events - Multiple subscribers", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("Multiple subscribers all receive StateChanged events") {
        int subscriber1_count = 0;
        int subscriber2_count = 0;
        int subscriber3_count = 0;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                subscriber1_count++;
            });

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                subscriber2_count++;
            });

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                subscriber3_count++;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // All three subscribers should have received the same number of events
        REQUIRE(subscriber1_count >= 2);  // At least Off→Booting and Booting→Active
        REQUIRE(subscriber1_count == subscriber2_count);
        REQUIRE(subscriber2_count == subscriber3_count);
    }

    SECTION("Multiple subscribers on different event types all receive events") {
        bool state_received = false;
        bool progress_received = false;
        bool step_started_received = false;
        bool step_completed_received = false;
        bool sequence_completed_received = false;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                state_received = true;
            });

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress&) {
                progress_received = true;
            });

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int, const std::string&) {
                step_started_received = true;
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int, const std::string&, startup::StepStatus) {
                step_completed_received = true;
            });

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool, const std::string&) {
                sequence_completed_received = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(state_received);
        REQUIRE(progress_received);
        REQUIRE(step_started_received);
        REQUIRE(step_completed_received);
        REQUIRE(sequence_completed_received);
    }
}

TEST_CASE("Startup | Events - Unsubscribe", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("Unsubscribed handler does not receive events") {
        int active_count = 0;
        int removed_count = 0;

        // This subscriber will remain active
        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                active_count++;
            });

        // This subscriber will be removed before the sequence runs
        auto subscription_id = manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                removed_count++;
            });

        // Remove the second subscriber BEFORE running
        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .remove(subscription_id);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(active_count >= 2);  // Active subscriber received events
        REQUIRE(removed_count == 0);  // Removed subscriber received NOTHING
    }

    SECTION("clear() removes all subscribers") {
        int count = 0;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                count++;
            });

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                count++;
            });

        // Clear all subscribers
        manager.events().get<startup::StartupManager::Event::StateChanged>().clear();

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(count == 0);  // No events received after clear()
    }
}

TEST_CASE("Startup | Events - Abort behavior", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("Aborted steps do not fire StepStarted/StepCompleted") {
        std::vector<std::string> event_log;

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int index, const std::string& id) {
                event_log.push_back("START:" + id);
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int index, const std::string& id, startup::StepStatus status) {
                event_log.push_back("COMPLETE:" + id + ":" + std::to_string(static_cast<int>(status)));
            });

        // Use a slow step so we have time to abort
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow_step", .display_name = "Slow Step", .timeout = std::chrono::seconds{10}}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "never_reached", .display_name = "Never Reached", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit then abort
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        manager.request_abort();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have started slow_step
        bool slow_started = false;
        bool never_reached_started = false;

        for (const auto& evt : event_log) {
            if (evt.find("START:slow_step") != std::string::npos) slow_started = true;
            if (evt.find("START:never_reached") != std::string::npos) never_reached_started = true;
        }

        REQUIRE(slow_started);
        REQUIRE_FALSE(never_reached_started);  // This step should never have started
    }

    SECTION("StateChanged fires correctly when aborted (Booting to Off)") {
        std::vector<std::pair<startup::AppState, startup::AppState>> state_changes;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                state_changes.push_back({old_s, new_s});
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow_step", .display_name = "Slow Step", .timeout = std::chrono::seconds{10}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit then abort
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        manager.request_abort();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have: Off→Booting, then Booting→Off (due to abort)
        REQUIRE(state_changes.size() >= 2);
        REQUIRE(state_changes[0].first == startup::AppState::Off);
        REQUIRE(state_changes[0].second == startup::AppState::Booting);
        REQUIRE(state_changes[1].first == startup::AppState::Booting);
        REQUIRE(state_changes[1].second == startup::AppState::Off);
    }

    SECTION("SequenceCompleted fires with success=false when aborted") {
        bool sequence_completed_fired = false;
        bool success_value = true;  // Start with true to verify it changes
        std::string error_msg;

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool success, const std::string& msg) {
                sequence_completed_fired = true;
                success_value = success;
                error_msg = msg;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow_step", .display_name = "Slow Step", .timeout = std::chrono::seconds{10}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit then abort
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        manager.request_abort();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(sequence_completed_fired);
        REQUIRE(success_value == false);  // Abort means failure
    }
}

TEST_CASE("Startup | Events - SequenceCompleted", "[startup]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("SequenceCompleted fires with success=true on successful completion") {
        bool event_fired = false;
        bool success = false;
        std::string error;

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool s, const std::string& e) {
                event_fired = true;
                success = s;
                error = e;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(success == true);
        REQUIRE(error.empty());
    }

    SECTION("SequenceCompleted fires with success=false and error message on failure") {
        bool event_fired = false;
        bool success = true;
        std::string error;

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool s, const std::string& e) {
                event_fired = true;
                success = s;
                error = e;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "fail_step", .display_name = "Fail Step", .timeout = std::chrono::seconds{5}, .critical = true},
            startup::StepResult{startup::StepStatus::Failed, "Critical error"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(success == false);
        REQUIRE_FALSE(error.empty());
    }
}