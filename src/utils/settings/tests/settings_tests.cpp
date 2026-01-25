//
// Created by Nicolas Désilles on 02/01/2026.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>
#include <iostream>
#include <thread>
#include <atomic>

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

TEST_CASE("Settings | Initialisation", "[settings]") {

    SECTION("initialization once works") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");

        settings::Settings test_settings;

        auto config1 = settings::SettingsConfig{temp_dir.path(), "test1.json", 1};
        test_settings.init(test_logger, config1);

        SUCCEED();

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("re-initialization works") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");

        settings::Settings test_settings;

        auto config1 = settings::SettingsConfig{temp_dir.path(), "test1.json", 1};
        test_settings.init(test_logger, config1);
        REQUIRE(test_settings.path() == temp_dir.path() / "test1.json");

        auto config2 = settings::SettingsConfig{temp_dir.path(), "test2.json", 2};
        test_settings.init(test_logger, config2);
        REQUIRE(test_settings.path() == temp_dir.path() / "test2.json");

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("is_initialized reflects initialization state") {
        settings::Settings test_settings;
        CHECK_FALSE(test_settings.is_initialized());

        log::init();
        auto test_logger = log::get("test");
        auto config = settings::SettingsConfig{fs::temp_directory_path(), "test", 1};

        test_settings.init(test_logger, config);
        CHECK(test_settings.is_initialized());

        test_settings.shutdown();
        CHECK_FALSE(test_settings.is_initialized());
        log::shutdown();
    }

    SECTION("calling methods before init returns error or false") {
        settings::Settings test_settings;

        CHECK_FALSE(test_settings.has_pending_changes());
        CHECK_FALSE(test_settings.load_or_create().ok);
        CHECK_FALSE(test_settings.save().result.ok);
        CHECK_FALSE(test_settings.export_to_file("some_path").ok);
        CHECK_FALSE(test_settings.import_from_file("some_path").ok);
        CHECK_FALSE(test_settings.reset_pending_to_active().ok);
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

    SECTION("parsing an invalid JSON string returns an error") {
        settings::AppSettings out{};
        auto result = settings::from_json_string("{", out);
        REQUIRE_FALSE(result.ok);
    }

    SECTION("parsing a valid JSON string with type mismatch returns an error") {
        settings::AppSettings out{};
        // general should be an object, not a number
        const std::string bad = R"({"schema_version":1,"general":123,"audio":{"sampling_rate":48000,"buffer_size":256}})";
        auto result = settings::from_json_string(bad, out);
        REQUIRE_FALSE(result.ok);
    }

    SECTION("missing categories preserve defaults") {
        settings::AppSettings out{};
        // No audio provided => should remain defaults
        const std::string partial = R"({"schema_version":1,"general":{"log_level":"info"}})";
        auto result = settings::from_json_string(partial, out);
        REQUIRE(result.ok);
        REQUIRE(out.general.log_level == "info");
        REQUIRE(out.audio.sampling_rate == 48000);
        REQUIRE(out.audio.buffer_size == 256);
    }

    SECTION("unknown keys inside nested objects are ignored") {
        settings::AppSettings out{};
        const std::string nested_unknown = R"({"schema_version":1,"general":{"log_level":"info","unknown_nested":999},"audio":{"sampling_rate":44100,"buffer_size":512,"mystery":true}})";
        auto result = settings::from_json_string(nested_unknown, out);
        REQUIRE(result.ok);
        REQUIRE(out.general.log_level == "info");
        REQUIRE(out.audio.sampling_rate == 44100);
        REQUIRE(out.audio.buffer_size == 512);
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

    SECTION("reading a non-existent JSON file returns an error") {
        TempDir temp_dir;

        const fs::path missing = temp_dir.path() / "missing.json";
        settings::AppSettings out{};
        auto result = settings::from_json_file(missing, out);
        REQUIRE_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("does not exist"));
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

TEST_CASE("Settings | Load from file or Create", "[settings]") {

    SECTION("if there is no existing settings file in base_dir, creates file with defaults + publishes defaults") {

        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings.json",
            .schema_version = 1
        };

        auto test_settings = settings::Settings();

        REQUIRE_NOTHROW(test_settings.init(test_logger, config));

        // verify that there is NO initial settings file before loading
        REQUIRE_FALSE(fs::exists(test_settings.path()));

        auto load_result = test_settings.load_or_create();

        REQUIRE(load_result.ok);

        REQUIRE(fs::exists(test_settings.path()));

        auto snapshot = test_settings.snapshot();

        REQUIRE(snapshot != nullptr);
        CHECK(snapshot->schema_version == 1);
        CHECK(snapshot->general.log_level == "debug");
        CHECK(snapshot->audio.sampling_rate == 48000);
        CHECK(snapshot->audio.buffer_size == 256);

        test_settings.shutdown();

        log::shutdown();

    }

    SECTION("loads an existing settings file correctly") {

        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");

        const std::string test_json_string = R"({"audio":{"buffer_size":512,"sampling_rate":44100},"general":{"log_level":"trace"},"schema_version":1})";

        auto test_file_path = temp_dir.path() / "aknet_test_settings.json";
        std::ofstream out_file(test_file_path);

        REQUIRE(out_file.is_open());

        out_file << test_json_string;
        out_file.close();

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings.json",
            .schema_version = 1
        };

        auto test_settings = settings::Settings();

        REQUIRE_NOTHROW(test_settings.init(test_logger, config));

        auto load_result = test_settings.load_or_create();

        REQUIRE(load_result.ok);

        auto snapshot = test_settings.snapshot();

        REQUIRE(snapshot != nullptr);
        CHECK(snapshot->schema_version == 1);
        CHECK(snapshot->general.log_level == "trace");
        CHECK(snapshot->audio.sampling_rate == 44100);
        CHECK(snapshot->audio.buffer_size == 512);

        test_settings.shutdown();

        log::shutdown();

    }

    SECTION("loading an invalid settings file returns an error and snapshot stays intact") {

        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");

        const std::string test_json_string = "{";

        auto test_file_path = temp_dir.path() / "aknet_test_settings.json";
        std::ofstream out_file(test_file_path);

        REQUIRE(out_file.is_open());

        out_file << test_json_string;
        out_file.close();

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings.json",
            .schema_version = 1
        };

        auto test_settings = settings::Settings();

        REQUIRE_NOTHROW(test_settings.init(test_logger, config));

        auto load_result = test_settings.load_or_create();

        REQUIRE_FALSE(load_result.ok);

        auto snapshot = test_settings.snapshot();

        REQUIRE(snapshot != nullptr);
        CHECK(snapshot->schema_version == 1);
        CHECK(snapshot->general.log_level == "debug");
        CHECK(snapshot->audio.sampling_rate == 48000);
        CHECK(snapshot->audio.buffer_size == 256);

        test_settings.shutdown();

        log::shutdown();

    }

    SECTION("schema version mismatch still loads settings") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");

        // schema_version intentionally mismatched
        const std::string test_json_string = R"({"audio":{"buffer_size":1024,"sampling_rate":32000},"general":{"log_level":"info"},"schema_version":999})";

        auto test_file_path = temp_dir.path() / "aknet_test_settings.json";
        std::ofstream out_file(test_file_path);
        REQUIRE(out_file.is_open());
        out_file << test_json_string;
        out_file.close();

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings.json",
            .schema_version = 1
        };

        settings::Settings test_settings;
        test_settings.init(test_logger, config);

        auto load_result = test_settings.load_or_create();
        REQUIRE(load_result.ok);

        auto snapshot = test_settings.snapshot();
        REQUIRE(snapshot != nullptr);
        REQUIRE(snapshot->general.log_level == "info");
        REQUIRE(snapshot->audio.sampling_rate == 32000);
        REQUIRE(snapshot->audio.buffer_size == 1024);
        REQUIRE(snapshot->schema_version == 999);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("fails when settings file path points to a directory") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");

        // Create a directory with the exact name that would be used for settings file
        auto settings_dir_path = temp_dir.path() / "aknet_test_settings.json";
        fs::create_directories(settings_dir_path);

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings.json",  // This is actually a directory!
            .schema_version = 1
        };

        settings::Settings test_settings;
        test_settings.init(test_logger, config);

        auto result = test_settings.load_or_create();
        REQUIRE_FALSE(result.ok);

        test_settings.shutdown();
        log::shutdown();
    }

}

TEST_CASE("Settings | Pending and Save", "[settings]") {

    SECTION("staging changes does not affect snapshot until save") {
        TempDir temp_dir;

        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings_pending.json",
            .schema_version = 1
        };

        settings::Settings test_settings;
        test_settings.init(test_logger, config);

        REQUIRE(test_settings.load_or_create().ok);

        // Initial state
        REQUIRE_FALSE(test_settings.has_pending_changes());
        REQUIRE(test_settings.snapshot()->general.log_level == "debug");
        REQUIRE(test_settings.pending_copy().general.log_level == "debug");

        // Stage a change
        REQUIRE(test_settings.stage([](settings::AppSettings& s) {
            s.general.log_level = "trace";
        }).ok);

        REQUIRE(test_settings.has_pending_changes());

        // Snapshot unchanged
        REQUIRE(test_settings.snapshot()->general.log_level == "debug");

        // Pending changed
        REQUIRE(test_settings.pending_copy().general.log_level == "trace");

        // Save applies + persists
        auto save_result = test_settings.save();
        REQUIRE(save_result.result.ok);

        REQUIRE_FALSE(test_settings.has_pending_changes());
        REQUIRE(test_settings.snapshot()->general.log_level == "trace");
        REQUIRE(test_settings.pending_copy().general.log_level == "trace");

        settings::AppSettings disk_settings{};
        REQUIRE(settings::from_json_file(test_settings.path(), disk_settings).ok);
        REQUIRE(disk_settings.general.log_level == "trace");

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("reset_pending_to_active discards staged changes") {
        TempDir temp_dir;

        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings_reset.json",
            .schema_version = 1
        };

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        REQUIRE(test_settings.stage([](settings::AppSettings& s) {
            s.audio.sampling_rate = 44100;
        }).ok);

        REQUIRE(test_settings.has_pending_changes());
        REQUIRE(test_settings.snapshot()->audio.sampling_rate == 48000);
        REQUIRE(test_settings.pending_copy().audio.sampling_rate == 44100);

        REQUIRE(test_settings.reset_pending_to_active().ok);
        REQUIRE_FALSE(test_settings.has_pending_changes());
        REQUIRE(test_settings.pending_copy().audio.sampling_rate == test_settings.snapshot()->audio.sampling_rate);
        REQUIRE(test_settings.snapshot()->audio.sampling_rate == 48000);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("staging a change and staging back removes pending changes") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{temp_dir.path(), "toggle.json", 1};
        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        REQUIRE_FALSE(test_settings.has_pending_changes());

        REQUIRE(test_settings.stage([](settings::AppSettings& s) {
            s.general.log_level = "trace";
        }).ok);
        REQUIRE(test_settings.has_pending_changes());

        REQUIRE(test_settings.stage([](settings::AppSettings& s) {
            s.general.log_level = "debug";
        }).ok);

        REQUIRE_FALSE(test_settings.has_pending_changes());

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("save with no changes has empty impact") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        auto save_result = test_settings.save();
        CHECK(save_result.result.ok);
        CHECK_FALSE(save_result.save_impact.app_restart_required);
        CHECK(save_result.save_impact.modules_restart_required.empty());
        CHECK(save_result.save_impact.restart_sensitive_keys_changed.empty());

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("overwriting when .bak already exists removes old .bak first") {
        TempDir temp_dir;
        auto file_path = temp_dir.path() / "test.json";
        auto bak_path = temp_dir.path() / "test.json.bak";

        // Create initial file
        settings::AppSettings initial{};
        auto r1 = settings::to_json_file(file_path, initial);
        REQUIRE(r1.ok);

        // Manually create a .bak file
        std::ofstream bak_file(bak_path);
        bak_file << "old backup content";
        bak_file.close();
        REQUIRE(fs::exists(bak_path));

        // Overwrite the settings file
        settings::AppSettings modified{};
        modified.general.log_level = "trace";
        auto r2 = settings::to_json_file(file_path, modified);
        REQUIRE(r2.ok);

        // Verify .bak was cleaned up after save
        CHECK_FALSE(fs::exists(bak_path));
    }

    SECTION("save fails when file cannot be written") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");

        settings::Settings test_settings;
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};
        test_settings.init(test_logger, config);
        test_settings.load_or_create();

        // Make the directory read-only to prevent writes
        fs::permissions(temp_dir.path(), fs::perms::owner_read | fs::perms::owner_exec);

        test_settings.stage([](settings::AppSettings& s) {
            s.general.log_level = "trace";
        });

        auto result = test_settings.save();

        // Restore permissions
        fs::permissions(temp_dir.path(), fs::perms::owner_all);

        REQUIRE_FALSE(result.result.ok);

        test_settings.shutdown();
        log::shutdown();
    }
}

TEST_CASE("Settings | Import and Export", "[settings]") {

    SECTION("export writes active snapshot, import stages only until save") {
        TempDir temp_dir;

        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings_main.json",
            .schema_version = 1
        };

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        // Make active non-default
        REQUIRE(test_settings.stage([](settings::AppSettings& s) {
            s.general.log_level = "trace";
            s.audio.sampling_rate = 44100;
        }).ok);
        {
            auto save_result = test_settings.save();
            REQUIRE(save_result.result.ok);
        }

        REQUIRE(test_settings.snapshot()->general.log_level == "trace");
        REQUIRE(test_settings.snapshot()->audio.sampling_rate == 44100);

        // Export current snapshot
        const fs::path export_path = temp_dir.path() / "exported_settings.json";
        REQUIRE(test_settings.export_to_file(export_path).ok);
        REQUIRE(fs::exists(export_path));

        settings::AppSettings exported{};
        REQUIRE(settings::from_json_file(export_path, exported).ok);
        REQUIRE(exported.general.log_level == "trace");
        REQUIRE(exported.audio.sampling_rate == 44100);

        // Prepare an import file with different values
        settings::AppSettings to_import{};
        to_import.general.log_level = "info";
        to_import.audio.sampling_rate = 32000;
        to_import.audio.buffer_size = 1024;

        const fs::path import_path = temp_dir.path() / "import_settings.json";
        REQUIRE(settings::to_json_file(import_path, to_import).ok);

        // Import should update pending only
        REQUIRE(test_settings.import_from_file(import_path).ok);
        REQUIRE(test_settings.has_pending_changes());

        REQUIRE(test_settings.snapshot()->general.log_level == "trace");
        REQUIRE(test_settings.snapshot()->audio.sampling_rate == 44100);

        auto pending = test_settings.pending_copy();
        REQUIRE(pending.general.log_level == "info");
        REQUIRE(pending.audio.sampling_rate == 32000);
        REQUIRE(pending.audio.buffer_size == 1024);

        // Save applies import to active + persists to main settings path
        {
            auto save_result = test_settings.save();
            REQUIRE(save_result.result.ok);
        }
        REQUIRE_FALSE(test_settings.has_pending_changes());

        REQUIRE(test_settings.snapshot()->general.log_level == "info");
        REQUIRE(test_settings.snapshot()->audio.sampling_rate == 32000);
        REQUIRE(test_settings.snapshot()->audio.buffer_size == 1024);

        settings::AppSettings disk_settings{};
        REQUIRE(settings::from_json_file(test_settings.path(), disk_settings).ok);
        REQUIRE(disk_settings.general.log_level == "info");
        REQUIRE(disk_settings.audio.sampling_rate == 32000);
        REQUIRE(disk_settings.audio.buffer_size == 1024);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("importing invalid JSON file returns error") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};

        fs::path invalid_json = temp_dir.path() / "invalid.json";
        std::ofstream out(invalid_json);
        out << "{ invalid json }";
        out.close();

        settings::Settings test_settings;
        test_settings.init(test_logger, config);

        auto result = test_settings.import_from_file(invalid_json);
        CHECK_FALSE(result.ok);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("exporting to invalid path returns error") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};

        settings::Settings test_settings;
        test_settings.init(test_logger, config);

        // Exporting to a directory path instead of a file path might fail depending on OS/implementation
        // but here we try to export where a directory already exists
        fs::path dir_path = temp_dir.path() / "some_dir";
        fs::create_directory(dir_path);

        auto result = test_settings.export_to_file(dir_path);
        // Note: filesystem::rename or ofstream might fail when target is a directory
        CHECK_FALSE(result.ok);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("importing from non-existent file returns error") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};

        settings::Settings test_settings;
        test_settings.init(test_logger, config);

        auto result = test_settings.import_from_file(temp_dir.path() / "non_existent.json");
        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("does not exist"));

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("exporting to nested directories creates parent directories") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{temp_dir.path(), "main.json", 1};
        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        fs::path export_path = temp_dir.path() / "nested" / "a" / "b" / "export.json";
        auto export_result = test_settings.export_to_file(export_path);
        REQUIRE(export_result.ok);
        REQUIRE(fs::exists(export_path));

        settings::AppSettings exported{};
        REQUIRE(settings::from_json_file(export_path, exported).ok);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("importing from non-existent file does not modify pending") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{temp_dir.path(), "main.json", 1};
        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        REQUIRE_FALSE(test_settings.has_pending_changes());
        auto pending_before = test_settings.pending_copy();

        auto result = test_settings.import_from_file(temp_dir.path() / "missing.json");
        REQUIRE_FALSE(result.ok);
        REQUIRE_FALSE(test_settings.has_pending_changes());
        auto pending_after = test_settings.pending_copy();

        REQUIRE(pending_after.general.log_level == pending_before.general.log_level);
        REQUIRE(pending_after.audio.sampling_rate == pending_before.audio.sampling_rate);
        REQUIRE(pending_after.audio.buffer_size == pending_before.audio.buffer_size);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("importing partial settings overlays defaults for missing fields") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{temp_dir.path(), "main.json", 1};
        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        const std::string partial_json = R"({"schema_version":1,"general":{"log_level":"info"}})";
        fs::path import_path = temp_dir.path() / "partial.json";
        std::ofstream out(import_path);
        REQUIRE(out.is_open());
        out << partial_json;
        out.close();

        REQUIRE(test_settings.import_from_file(import_path).ok);

        auto pending = test_settings.pending_copy();
        REQUIRE(pending.general.log_level == "info");
        REQUIRE(pending.audio.sampling_rate == 48000);
        REQUIRE(pending.audio.buffer_size == 256);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("writing to a read-only directory fails gracefully") {
        TempDir temp_dir;
        auto readonly_dir = temp_dir.path() / "readonly";
        fs::create_directories(readonly_dir);

        // Make directory read-only
        fs::permissions(readonly_dir, fs::perms::owner_read | fs::perms::owner_exec);

        auto file_path = readonly_dir / "test.json";
        settings::AppSettings test_settings{};

        auto result = settings::to_json_file(file_path, test_settings);

        // Restore permissions for cleanup
        fs::permissions(readonly_dir, fs::perms::owner_all);

        REQUIRE_FALSE(result.ok);
    }
}

TEST_CASE("Settings | Restart Rules and Impact", "[settings]") {

    SECTION("save computes impact for restart-sensitive keys") {
        TempDir temp_dir;

        log::init();
        auto test_logger = log::get("settings");

        auto config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .file_name = "aknet_test_settings_restart_rules.json",
            .schema_version = 1
        };

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        // Define a couple of manual rules
        test_settings.add_restart_rule(settings::RestartRule{
            .key = "general.test_restart_impact",
            .requires_app_restart = true,
            .module_name_to_restart = ""
        });
        test_settings.add_restart_rule(settings::RestartRule{
            .key = "audio.buffer_size",
            .requires_app_restart = false,
            .module_name_to_restart = "audio"
        });

        auto rules = test_settings.get_restart_rules();
        REQUIRE(rules.size() >= 2);

        // Changing a non-sensitive key has no impact
        REQUIRE(test_settings.stage([](settings::AppSettings& s) {
            s.general.log_level = "trace";
        }).ok);

        auto save1 = test_settings.save();
        REQUIRE(save1.result.ok);
        REQUIRE_FALSE(save1.save_impact.app_restart_required);
        REQUIRE(save1.save_impact.modules_restart_required.empty());
        REQUIRE(save1.save_impact.restart_sensitive_keys_changed.empty());

        // Changing restart-sensitive keys has an impact
        REQUIRE(test_settings.stage([](settings::AppSettings& s) {
            s.general.test_restart_impact = 42;
            s.audio.buffer_size = 2048;
        }).ok);

        auto save2 = test_settings.save();
        REQUIRE(save2.result.ok);

        REQUIRE(save2.save_impact.app_restart_required);
        REQUIRE(std::find(save2.save_impact.modules_restart_required.begin(),
                          save2.save_impact.modules_restart_required.end(),
                          "audio") != save2.save_impact.modules_restart_required.end());

        REQUIRE(std::find(save2.save_impact.restart_sensitive_keys_changed.begin(),
                          save2.save_impact.restart_sensitive_keys_changed.end(),
                          "general.test_restart_impact") != save2.save_impact.restart_sensitive_keys_changed.end());
        REQUIRE(std::find(save2.save_impact.restart_sensitive_keys_changed.begin(),
                          save2.save_impact.restart_sensitive_keys_changed.end(),
                          "audio.buffer_size") != save2.save_impact.restart_sensitive_keys_changed.end());

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("multiple restart rules for the same key are handled") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        test_settings.add_restart_rule({"audio.buffer_size", false, "audio"});
        test_settings.add_restart_rule({"audio.buffer_size", true, ""}); // Also requires app restart

        test_settings.stage([](auto& s){ s.audio.buffer_size = 1024; });

        auto save_result = test_settings.save();
        CHECK(save_result.result.ok);
        CHECK(save_result.save_impact.app_restart_required);
        CHECK_THAT(save_result.save_impact.modules_restart_required, Catch::Matchers::VectorContains(std::string("audio")));
        CHECK_THAT(save_result.save_impact.restart_sensitive_keys_changed, Catch::Matchers::VectorContains(std::string("audio.buffer_size")));
        // Should only appear once even if multiple rules match
        CHECK(save_result.save_impact.restart_sensitive_keys_changed.size() == 1);

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("restart rule with empty module_name_to_restart does not add to modules list") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");

        settings::Settings test_settings;
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};
        test_settings.init(test_logger, config);
        test_settings.load_or_create();

        // Add rule with empty module_name (but key and no app restart)
        test_settings.add_restart_rule({
            .key = "general.log_level",
            .requires_app_restart = false,
            .module_name_to_restart = ""  // Empty!
        });

        test_settings.stage([](settings::AppSettings& s) {
            s.general.log_level = "trace";
        });

        auto result = test_settings.save();
        REQUIRE(result.result.ok);
        CHECK(result.save_impact.modules_restart_required.empty());
        CHECK_FALSE(result.save_impact.restart_sensitive_keys_changed.empty());

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("unknown restart-rule key never triggers impact") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        test_settings.add_restart_rule({"does.not.exist", true, "some_module"});

        REQUIRE(test_settings.stage([](auto& s){ s.general.log_level = "trace"; }).ok);

        auto save_result = test_settings.save();
        REQUIRE(save_result.result.ok);
        REQUIRE_FALSE(save_result.save_impact.app_restart_required);
        REQUIRE(save_result.save_impact.modules_restart_required.empty());
        REQUIRE(save_result.save_impact.restart_sensitive_keys_changed.empty());

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("get_restart_rules returns a copy") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("test");
        auto config = settings::SettingsConfig{temp_dir.path(), "test.json", 1};

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        test_settings.add_restart_rule({"audio.buffer_size", false, "audio"});

        auto rules1 = test_settings.get_restart_rules();
        REQUIRE_FALSE(rules1.empty());
        rules1.clear();

        auto rules2 = test_settings.get_restart_rules();
        REQUIRE_FALSE(rules2.empty());

        test_settings.shutdown();
        log::shutdown();
    }
}

TEST_CASE("Settings | Concurrency", "[settings]") {

    SECTION("concurrent snapshot reads while staging does not crash") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");
        auto config = settings::SettingsConfig{temp_dir.path(), "concurrent.json", 1};

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        std::atomic<bool> stop{false};

        std::thread reader([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                auto snap = test_settings.snapshot();
                if (snap) {
                    (void)snap->general.log_level;
                    (void)snap->audio.sampling_rate;
                }
            }
        });

        std::thread writer([&] {
            for (int i = 0; i < 2000; ++i) {
                test_settings.stage([&](settings::AppSettings& s) {
                    s.general.test_restart_impact = i;
                });
            }
            stop.store(true, std::memory_order_relaxed);
        });

        writer.join();
        reader.join();

        test_settings.shutdown();
        log::shutdown();
    }

    SECTION("concurrent add_restart_rule while saving does not crash") {
        TempDir temp_dir;
        log::init();
        auto test_logger = log::get("settings");
        auto config = settings::SettingsConfig{temp_dir.path(), "concurrent_save.json", 1};

        settings::Settings test_settings;
        test_settings.init(test_logger, config);
        REQUIRE(test_settings.load_or_create().ok);

        std::atomic<bool> done{false};

        std::thread rule_adder([&] {
            for (int i = 0; i < 200; ++i) {
                test_settings.add_restart_rule({"general.test_restart_impact", false, ""});
            }
            done.store(true, std::memory_order_relaxed);
        });

        std::thread saver([&] {
            for (int i = 0; i < 50; ++i) {
                test_settings.stage([&](settings::AppSettings& s) {
                    s.general.test_restart_impact = i;
                });
                (void)test_settings.save();
            }
        });

        saver.join();
        rule_adder.join();

        CHECK(done.load(std::memory_order_relaxed));

        test_settings.shutdown();
        log::shutdown();
    }
}