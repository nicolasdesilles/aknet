//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "test_fixtures.h"
#include <startup_steps/check_jack_installation_step.h>
#include <startup_step.h>
#include <settings.h>
#include <filesystem>
#include <fstream>

using namespace aknet;
using namespace aknet::test;

namespace fs = std::filesystem;

struct CheckJackStepTestFixture {
    std::shared_ptr<log::Logger> logger_settings;
    std::shared_ptr<log::Logger> logger_startup;
    std::unique_ptr<settings::Settings> settings;
    TempDir temp_dir;

    CheckJackStepTestFixture() {
        log::init(temp_dir.path());
        logger_settings = log::get("settings");
        logger_startup = log::get("startup_test");

        settings = std::make_unique<settings::Settings>();
        settings->init(logger_settings, settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .schema_version = 1
        });
        settings->load_or_create();
    }

    ~CheckJackStepTestFixture() {
        settings->shutdown();
        log::shutdown();
    }

    startup::StepContext create_context() {
        startup::StepContext ctx;
        ctx.logger = logger_startup;
        ctx.settings = settings.get();
        return ctx;
    }
};

TEST_CASE("Jack | CheckJackInstallationStep - Config", "[jack][startup]") {
    jack::CheckJackInstallationStep step;

    SECTION("step has correct config") {
        const auto& config = step.config();

        CHECK(config.id == "check_jack_installation");
        CHECK(config.display_name == "Check JACK Installation");
        CHECK(config.timeout == std::chrono::seconds{5});
        CHECK(config.critical == true);
        CHECK(config.can_skip == false);
    }
}

TEST_CASE("Jack | CheckJackInstallationStep - Execution", "[jack][startup]") {
    CheckJackStepTestFixture f;
    jack::CheckJackInstallationStep step;

    SECTION("step succeeds when jackd exists at default path") {
        // Use real path to jackd on macOS
        f.settings->stage([](settings::AppSettings& s) {
            s.jack.server_executable_path = "/opt/homebrew/bin/jackd";
        });
        f.settings->save();

        auto ctx = f.create_context();
        auto result = step.run(ctx);

        // Only check if file actually exists - test is system-dependent
        if (fs::exists("/opt/homebrew/bin/jackd")) {
            CHECK(result.status == startup::StepStatus::Success);
            CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("JACK found"));
        }
    }

    SECTION("step succeeds when path is a directory containing jackd") {
        // Create a temp directory with a fake jackd executable
        auto bin_dir = f.temp_dir.path() / "fake_bin";
        fs::create_directories(bin_dir);

        auto jackd_path = bin_dir / "jackd";
        std::ofstream fake_jackd(jackd_path);
        fake_jackd << "#!/bin/sh\n";
        fake_jackd.close();

        // Make it executable
        fs::permissions(jackd_path,
            fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec);

        // Configure with directory path (not full executable path)
        f.settings->stage([&bin_dir](settings::AppSettings& s) {
            s.jack.server_executable_path = bin_dir.string();
        });
        f.settings->save();

        auto ctx = f.create_context();
        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Success);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring(jackd_path.string()));
    }

    SECTION("step fails when jackd does not exist") {
        f.settings->stage([](settings::AppSettings& s) {
            s.jack.server_executable_path = "/nonexistent/path/jackd";
        });
        f.settings->save();

        auto ctx = f.create_context();
        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Failed);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("not found"));
    }

    SECTION("step fails when path is empty") {
        f.settings->stage([](settings::AppSettings& s) {
            s.jack.server_executable_path = "";
        });
        f.settings->save();

        auto ctx = f.create_context();
        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Failed);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("empty"));
    }

    SECTION("step fails when directory does not contain jackd") {
        // Create a temp directory WITHOUT jackd
        auto empty_dir = f.temp_dir.path() / "empty_bin";
        fs::create_directories(empty_dir);

        f.settings->stage([&empty_dir](settings::AppSettings& s) {
            s.jack.server_executable_path = empty_dir.string();
        });
        f.settings->save();

        auto ctx = f.create_context();
        auto result = step.run(ctx);

        CHECK(result.status == startup::StepStatus::Failed);
        CHECK_THAT(result.message, Catch::Matchers::ContainsSubstring("not found"));
    }
}