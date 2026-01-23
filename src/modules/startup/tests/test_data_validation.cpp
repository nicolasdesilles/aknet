//
// Created by Nicolas Désilles on 23/01/2026.
//

#include "test_fixtures.h"

using namespace aknet;
using namespace aknet::test;

// ------------------------------------------------------------------------------------------------
// Data Validation Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Startup | Data validation", "[startup][validation]") {

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

TEST_CASE("Startup | Data validation edge cases", "[startup][validation]") {

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

TEST_CASE("Startup | Helper functions", "[startup][validation]") {

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

TEST_CASE("Startup | Default helpers", "[startup][validation]") {

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
