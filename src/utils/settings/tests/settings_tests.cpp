//
// Created by Nicolas Désilles on 02/01/2026.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

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
        auto test_settings = settings::Settings();
        
        CHECK_THROWS_AS(test_settings.init(nullptr, test_config), std::invalid_argument);

        test_settings.shutdown();
    }

    SECTION("creating settings without a base_dir specified in config throws") {

        log::init();
        auto test_logger = log::get("test");

        // Empty path should trigger an error during initialization
        auto invalid_config = settings::SettingsConfig{fs::path(""), "test", 1};
        auto test_settings = settings::Settings();

        CHECK_THROWS_AS(test_settings.init(test_logger, invalid_config), std::invalid_argument);

        test_settings.shutdown();

        log::shutdown();

    }

    SECTION("creating settings with an empty file name specified in config succeeds and uses aknet_settings.json as default value") {

        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");

        // Providing an empty string for the filename
        auto config_empty_name = settings::SettingsConfig{temp_dir.path(), "", 1};

        auto test_settings = settings::Settings();

        REQUIRE_NOTHROW(test_settings.init(test_logger, config_empty_name));

        // Verify that it defaults to "aknet_settings.json"
        REQUIRE(test_settings.path() == (temp_dir.path() / "aknet_settings.json"));

        test_settings.shutdown();

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

        auto test_settings = settings::Settings();

        REQUIRE_NOTHROW(test_settings.init(test_logger, config_no_name));

        // Verify that it defaults to "aknet_settings.json"
        REQUIRE(test_settings.path() == (temp_dir.path() / "aknet_settings.json"));

        test_settings.shutdown();

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

        auto test_settings = settings::Settings();

        test_settings.init(test_logger, test_config);

        auto snapshot = test_settings.snapshot();

        REQUIRE(snapshot != nullptr);
        CHECK(snapshot->schema_version == 1);
        CHECK(snapshot->general.log_level == "debug");
        CHECK(snapshot->audio.sampling_rate == 48000);
        CHECK(snapshot->audio.buffer_size == 256);

        test_settings.shutdown();

        log::shutdown();

    }
}

TEST_CASE("Settings | JSON Helpers", "[settings]") {

    SECTION("roundtrip of app settings in json strings works") {

        auto test_app_settings = settings::AppSettings{};

        auto json_str = settings::to_json_string(test_app_settings);
        CHECK(!json_str.empty());

        auto destination_app_settings = settings::AppSettings{};

        //Purposefully changing values
        destination_app_settings.general.log_level = "trace";
        destination_app_settings.audio.sampling_rate = 44100;
        destination_app_settings.audio.buffer_size = 512;

        settings::Result parse_results = settings::from_json_string(json_str,destination_app_settings);

        REQUIRE(parse_results.ok);

        REQUIRE(destination_app_settings.general.log_level == "debug");
        REQUIRE(destination_app_settings.audio.sampling_rate == 48000);
        REQUIRE(destination_app_settings.audio.buffer_size == 256);

    }

    SECTION("reading an AppSettings object from a JSON file works") {

        TempDir temp_dir;

        const std::string test_json_string = R"({"audio":{"buffer_size":256,"sampling_rate":48000},"general":{"log_level":"debug"},"schema_version":1})";

        auto test_file_path = temp_dir.path() / "test_json_settings.json";
        std::ofstream out_file(test_file_path);

        REQUIRE(out_file.is_open());

        out_file << test_json_string;
        out_file.close();

        auto destination_app_settings = settings::AppSettings{};

        //Purposefully changing values
        destination_app_settings.general.log_level = "trace";
        destination_app_settings.audio.sampling_rate = 44100;
        destination_app_settings.audio.buffer_size = 512;

        auto read_result = settings::from_json_file(test_file_path,destination_app_settings);

        REQUIRE(read_result.ok);

        REQUIRE(destination_app_settings.general.log_level == "debug");
        REQUIRE(destination_app_settings.audio.sampling_rate == 48000);
        REQUIRE(destination_app_settings.audio.buffer_size == 256);

    }

    SECTION("writing an AppSettings object to a JSON file creates file and it parses") {

        TempDir temp_dir;
        auto test_file_path = temp_dir.path() / "test_json_settings.json";
        auto test_app_settings = settings::AppSettings{};

        auto write_result = settings::to_json_file(test_file_path,test_app_settings);

        REQUIRE(write_result.ok);
        REQUIRE(fs::exists(test_file_path));

        auto read_app_settings = settings::AppSettings{};
        auto read_result = settings::from_json_file(test_file_path,read_app_settings);

        REQUIRE(read_result.ok);

        REQUIRE(read_app_settings.general.log_level == "debug");
        REQUIRE(read_app_settings.audio.sampling_rate == 48000);
        REQUIRE(read_app_settings.audio.buffer_size == 256);

    }

    SECTION("writing an AppSettings object to a JSON file creates parent directories") {

        TempDir temp_dir;
        auto test_file_path = temp_dir.path() / "nested_a" / "nested_b" / "test_json_settings.json";

        auto test_app_settings = settings::AppSettings{};

        auto write_result = settings::to_json_file(test_file_path,test_app_settings);

        REQUIRE(write_result.ok);
        REQUIRE(fs::exists(test_file_path));

    }

    SECTION("overwriting an AppSettings object to an existing JSON file works and it parses") {

        TempDir temp_dir;
        auto test_file_path = temp_dir.path() / "test_json_settings.json";
        auto test_app_settings = settings::AppSettings{};

        auto write_result = settings::to_json_file(test_file_path,test_app_settings);

        REQUIRE(write_result.ok);
        REQUIRE(fs::exists(test_file_path));

        test_app_settings.general.log_level = "trace";
        test_app_settings.audio.sampling_rate = 44100;

        write_result = settings::to_json_file(test_file_path,test_app_settings);

        REQUIRE(write_result.ok);
        REQUIRE(fs::exists(test_file_path));

        auto read_app_settings = settings::AppSettings{};
        auto read_result = settings::from_json_file(test_file_path,read_app_settings);

        REQUIRE(read_result.ok);

        REQUIRE(read_app_settings.general.log_level == "trace");
        REQUIRE(read_app_settings.audio.sampling_rate == 44100);
        REQUIRE(read_app_settings.audio.buffer_size == 256);

    }

    SECTION("reading an invalid JSON file returns an error") {

        TempDir temp_dir;

        const std::string test_json_string = "{";

        auto test_file_path = temp_dir.path() / "test_json_settings.json";
        std::ofstream out_file(test_file_path);

        REQUIRE(out_file.is_open());

        out_file << test_json_string;
        out_file.close();

        auto destination_app_settings = settings::AppSettings{};

        auto read_result = settings::from_json_file(test_file_path,destination_app_settings);

        REQUIRE_FALSE(read_result.ok);

    }

    SECTION("reading a valid JSON file with unknown keys works and unknown keys are ignored") {

        TempDir temp_dir;

        const std::string test_json_string = R"({"audio":{"buffer_size":256,"sampling_rate":48000},"general":{"log_level":"debug"},"schema_version":1,"unknown":true})";

        auto test_file_path = temp_dir.path() / "test_json_settings.json";
        std::ofstream out_file(test_file_path);

        REQUIRE(out_file.is_open());

        out_file << test_json_string;
        out_file.close();

        auto destination_app_settings = settings::AppSettings{};

        //Purposefully changing values
        destination_app_settings.general.log_level = "trace";
        destination_app_settings.audio.sampling_rate = 44100;
        destination_app_settings.audio.buffer_size = 512;

        auto read_result = settings::from_json_file(test_file_path,destination_app_settings);

        REQUIRE(read_result.ok);

        REQUIRE(destination_app_settings.general.log_level == "debug");
        REQUIRE(destination_app_settings.audio.sampling_rate == 48000);
        REQUIRE(destination_app_settings.audio.buffer_size == 256);

    }


}