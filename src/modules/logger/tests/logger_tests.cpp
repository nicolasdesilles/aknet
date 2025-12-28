//
// Created by Nicolas Désilles on 28/12/2025.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

#include <logger.h>

using namespace aknet::logger;
namespace fs = std::filesystem;

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

// Helper to create a temporary test directory
class TempDir {
    fs::path path_;
public:
    // Constructor
    TempDir() : path_(fs::temp_directory_path() / "aknet_test_logs") {
        fs::create_directories(path_);
    }
    // Destructor
    ~TempDir() {
        fs::remove_all(path_);
    }
    const fs::path& path() const { return path_; }
};

// ------------------------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Logger | Factory", "[logger][factory]") {

    SECTION("create_logger returns valid instance") {
        auto log = create_logger();
        REQUIRE(log != nullptr);
    }
}

TEST_CASE("Logger | Initialization", "[logger][init]") {

    SECTION("creates log file in specified directory") {
        const TempDir temp_dir;

        logger::init(temp_dir.path(),false); // file only
        LOG_INFO("test");
        logger::shutdown();

        // Check that the log file exists
        bool found_log_file = false;
        for (const auto& entry : fs::directory_iterator(temp_dir.path())) {
            if (entry.path().extension() == ".log") {
                found_log_file = true;
                break;
            }
        }
        REQUIRE(found_log_file);
    }

    SECTION("multiple init calls are properly handled") {
        const TempDir temp_dir;

        logger::init(temp_dir.path(),false); // file only
        logger::init(temp_dir.path(),false); // file only

        LOG_INFO("After double init");
        logger::shutdown();

        SUCCEED();
    }
}

TEST_CASE("Logger | Shutdown", "[logger][shutdown]") {

    SECTION("can shutdown without being initialized first") {
        logger::shutdown();
        SUCCEED();
    }

    SECTION("can be init after shutdown") {
        const TempDir temp_dir;

        logger::init(temp_dir.path(),false); // file only
        logger::shutdown();

        logger::init(temp_dir.path(),false); // file only
        LOG_INFO("Reinitialized");
        logger::shutdown();

        SUCCEED();
    }

}


