//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "test_fixtures.h"
#include <startup_steps/ensure_jack_server_step.h>
#include <jack_module.h>
#include <startup_step.h>
#include <settings.h>

using namespace aknet;
using namespace aknet::test;

struct EnsureJackServerStepTestFixture {
    std::shared_ptr<log::Logger> logger_settings;
    std::shared_ptr<log::Logger> logger_startup;
    std::shared_ptr<log::Logger> logger_jack;
    std::unique_ptr<settings::Settings> settings;
    std::shared_ptr<jack::JackModule> jack_module;
    TempDir temp_dir;

    EnsureJackServerStepTestFixture() {
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
    }

    ~EnsureJackServerStepTestFixture() {
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

TEST_CASE("Jack | EnsureJackServerStep - Config", "[jack][startup]") {
    jack::EnsureJackServerStep step;

    SECTION("step has correct config") {
        const auto& config = step.config();

        CHECK(config.id == "ensure_jack_server");
        CHECK(config.display_name == "Initialize JACK Module");
        CHECK(config.timeout == std::chrono::seconds{10});
        CHECK(config.critical == true);
        CHECK(config.can_skip == false);
    }
}

TEST_CASE("Jack | EnsureJackServerStep - Execution", "[jack][startup]") {
    EnsureJackServerStepTestFixture f;
    jack::EnsureJackServerStep step;

    SECTION("step succeeds with valid settings") {
        auto ctx = f.create_context();
        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Success);
        CHECK(f.jack_module->get_state() == jack::JackModuleState::Initialized);
    }

    SECTION("step fails if jack_module is null") {
        auto ctx = f.create_context();
        ctx.jack_module = nullptr;

        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Failed);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("not initialized"));
    }

    SECTION("step fails with invalid settings (zero channels)") {
        f.settings->stage([](settings::AppSettings& s) {
            s.audio.num_channels = 0;  // Invalid
        });
        f.settings->save();

        auto ctx = f.create_context();
        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Failed);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("channel"));
    }

    SECTION("step fails with empty client name") {
        f.settings->stage([](settings::AppSettings& s) {
            s.jack.client_name = "";  // Invalid
        });
        f.settings->save();

        auto ctx = f.create_context();
        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Failed);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("empty"));
    }
}