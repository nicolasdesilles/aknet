//
// Created by Nicolas Désilles on 02/01/2026.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>

#include <logger.h>
#include <settings.h>

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
    TempDir() : path_(fs::temp_directory_path() / "aknet_settings_tests") {
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


// ------------------------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Settings | Creation", "[settings]") {

    SECTION("creating settings without a valid logger throws") {
        auto test_config = settings::SettingsConfig{fs::temp_directory_path(), "test", 1};
        
        CHECK_THROWS_AS(settings::Settings(nullptr, test_config), std::invalid_argument);
    }

    SECTION("creating settings without a base_dir specified in config throws") {

        log::init();
        auto test_logger = log::get("test");

        // Empty path should trigger an error during initialization
        auto invalid_config = settings::SettingsConfig{fs::path(""), "test", 1};

        CHECK_THROWS_AS(settings::Settings(test_logger, invalid_config), std::invalid_argument);

        log::shutdown();

    }

    SECTION("creating settings with an empty file name specified in config succeeds and uses aknet_settings.json as default value") {

        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");

        // Providing an empty string for the filename
        auto config_empty_name = settings::SettingsConfig{temp_dir.path(), "", 1};

        settings::Settings test_settings(test_logger, config_empty_name);

        // Verify that it defaults to "aknet_settings.json"
        REQUIRE(test_settings.path() == (temp_dir.path() / "aknet_settings.json"));

        log::shutdown();

    }

    SECTION("creating settings with no file name specified in config succeeds and uses aknet_settings.json as default value") {

        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");

        // Providing no string for the filename
        auto config_no_name = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .schema_version = 1
        };

        settings::Settings test_settings(test_logger, config_no_name);

        // Verify that it defaults to "aknet_settings.json"
        REQUIRE(test_settings.path() == (temp_dir.path() / "aknet_settings.json"));

        log::shutdown();

    }

    SECTION("default settings snapshot has expected default values") {

        TempDir temp_dir;

        log::init();
        auto test_logger = log::get("test");

        auto test_config = settings::SettingsConfig{
            temp_dir.path(),
            "aknet_test_settings",
            1};

        auto test_settings = settings::Settings(
            test_logger,
            test_config);

        auto snapshot = test_settings.snapshot();

        REQUIRE(snapshot != nullptr);
        CHECK(snapshot->schema_version == 1);
        CHECK(snapshot->general.log_level == "info");
        CHECK(snapshot->audio.sampling_rate == 48000);
        CHECK(snapshot->audio.buffer_size == 256);

        log::shutdown();

    }
}