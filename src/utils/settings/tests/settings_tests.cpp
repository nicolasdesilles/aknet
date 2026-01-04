//
// Created by Nicolas Désilles on 02/01/2026.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>
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
}