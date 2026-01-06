//
// Created by Nicolas Désilles on 06/01/2026.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <chrono>
#include <vector>

#include "startup.h"

using namespace aknet;

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

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

    SECTION("step config with a timeout value of 0 fails validation") {

        startup::StepConfig step_config{
            .id = "test_id",
            .display_name = "Test Display Name",
            .timeout = std::chrono::seconds{0}};

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