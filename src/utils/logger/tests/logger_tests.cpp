//
// Created by Nicolas Désilles on 28/12/2025.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

#include <logger.h>

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

TEST_CASE("Logger | Initialisation", "[logger]") {

    SECTION("init creates log directory") {

        // Temp path to check if the directory is created when specified
        fs::path logs_dir = fs::path(fs::temp_directory_path() / "test_logs_dir");

        // Initialisation of the logging system
        log::init(logs_dir);

        REQUIRE(fs::exists(logs_dir));
    }
}



