//
// Created by Nicolas Désilles on 25/01/2026.
//

#include <startup_steps/register_jack_client_step.h>
#include <jack_module.h>
#include <startup_step.h>
#include <settings.h>

#include "helpers/test_fixtures.h"

using namespace aknet;
using namespace aknet::test;

struct RegisterJackClientStepTestFixture {
    std::shared_ptr<log::Logger> logger_settings;
    std::shared_ptr<log::Logger> logger_startup;
    std::shared_ptr<log::Logger> logger_jack;
    std::unique_ptr<settings::Settings> settings;
    std::shared_ptr<jack::JackModule> jack_module;
    std::shared_ptr<MockProcessRunner> process_runner;
    std::shared_ptr<MockJackClientAPI> client_api;
    TempDir temp_dir;

    RegisterJackClientStepTestFixture() {
        log::init(temp_dir.path());
        logger_settings = log::get("settings");
        logger_startup = log::get("startup_test");
        logger_jack = log::get("jack_test");

        settings = std::make_unique<settings::Settings>();
        settings->init(logger_settings, settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .schema_version = 1
        });
        settings->load_or_create();

        // Setup mocks
        process_runner = std::make_shared<MockProcessRunner>();
        client_api = std::make_shared<MockJackClientAPI>();
        client_api->set_probe_result({48000, 256, true});  // Server running

        // Setup default settings
        settings->stage([](settings::AppSettings& s) {
            s.jack.client_name = "aknet_test";
            s.jack.server_executable_path = "/opt/homebrew/bin/jackd";
            s.jack.auto_manage_server = true;
            s.audio.sampling_rate = 48000;
            s.audio.buffer_size = 256;
            s.audio.num_channels = 2;
        });

        // Create JACK module
        jack_module = std::make_shared<jack::JackModule>(logger_jack);

        // Initialize module with mocks
        jack_module->init(*settings->snapshot(), process_runner, client_api);
    }

    ~RegisterJackClientStepTestFixture() {
        settings->shutdown();
        log::shutdown();
    }

    startup::StepContext create_context() {
        startup::StepContext ctx;
        ctx.logger = logger_startup;
        ctx.settings = settings.get();
        ctx.jack_module = jack_module;
        return ctx;
    }
};

TEST_CASE("Jack | RegisterJackClientStep - Config", "[jack][startup]") {
    jack::RegisterJackClientStep step;

    SECTION("step has correct config") {
        const auto& config = step.config();

        CHECK(config.id == "register_jack_client");
        CHECK(config.display_name == "Register JACK Client");
        CHECK(config.timeout == std::chrono::seconds{10});
        CHECK(config.critical == true);
        CHECK(config.can_skip == false);
    }
}

TEST_CASE("Jack | RegisterJackClientStep - Execution", "[jack][startup]") {
    RegisterJackClientStepTestFixture f;
    jack::RegisterJackClientStep step;

    SECTION("step succeeds when module is initialized") {
        auto ctx = f.create_context();
        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Success);
        CHECK(f.jack_module->is_ready());
        CHECK(f.jack_module->get_state() == jack::JackModuleState::Active);
    }

    SECTION("step fails if jack_module is null") {
        auto ctx = f.create_context();
        ctx.jack_module = nullptr;

        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Failed);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("not initialized"));
    }

    SECTION("step fails if module not initialized") {
        // Create fresh module (not initialized)
        auto fresh_module = std::make_shared<jack::JackModule>(f.logger_jack);

        auto ctx = f.create_context();
        ctx.jack_module = fresh_module;

        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Failed);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("not initialized"));
    }

    SECTION("step verifies client is active after success") {
        auto ctx = f.create_context();
        auto result = step.run(ctx);

        REQUIRE(result.status == startup::StepStatus::Success);

        // Verify module state
        CHECK(f.jack_module->is_ready());

        // Verify client is actually active via mock
        CHECK(f.client_api->is_active());
    }
}