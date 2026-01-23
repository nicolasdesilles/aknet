//
// Created by Nicolas Désilles on 28/12/2025.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <thread>

#include <logger.h>

#include <spdlog/spdlog.h>

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

TEST_CASE("Logger | System Initialisation", "[logger]") {

    const TempDir temp_dir;

    SECTION("init creates log directory") {

        // Temp path to check if the directory is created when specified
        fs::path logs_dir = fs::path(fs::temp_directory_path() / "test_logs_dir");

        // Initialisation of the logging system
        log::init(logs_dir);

        REQUIRE(fs::exists(logs_dir));

        log::shutdown();
    }

    SECTION("init with a specified directory creates a log file there") {

        // Initialisation of the logging system
        log::init(temp_dir.path());

        bool found_log_file = false;
        for (const auto &entry: fs::directory_iterator( temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                break;
            }
        }

        REQUIRE(found_log_file);

        log::shutdown();

    }


    /*
     * WARNING: this test is only okay to do on macOS, and also because I have hardcoded the default logs dir
     * to "home/Desktop/Logs/aknet in the logger.cpp
     * --> TO UPDATE LATER!
     */
    SECTION("init without a specified directory creates a log file in the default location") {

        const char* home = std::getenv("HOME");
        if (!home) throw std::runtime_error("HOME environment variable not set") ;

        int initial_n_log_files = 0;
        for (const auto &entry: fs::directory_iterator( fs::path(fs::path(home) / "Desktop" / "Logs" / "aknet"))) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                initial_n_log_files++;
            }
        }

        // Initialisation of the logging system
        log::init();

        int new_n_log_files = 0;
        for (const auto &entry: fs::directory_iterator( fs::path(fs::path(home) / "Desktop" / "Logs" / "aknet"))) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                new_n_log_files++;
            }
        }

        REQUIRE(new_n_log_files == initial_n_log_files + 1);

        log::shutdown();

    }

    SECTION("calling init twice does not throw an error") {

        log::init(temp_dir.path());

        REQUIRE_NOTHROW(log::init(temp_dir.path()));

        log::shutdown();
    }

    SECTION("calling init after shutdown works") {

        log::init(temp_dir.path());

        log::shutdown();

        REQUIRE_NOTHROW(log::init(temp_dir.path()));

        log::shutdown();
    }

}

TEST_CASE("Logger | System Shutdown", "[logger]") {

    const TempDir temp_dir;

    SECTION("shutdown resets initialization state") {

        log::init(temp_dir.path());

        log::shutdown();

        bool is_initialized = log::is_initialized();

        REQUIRE_FALSE(is_initialized);
    }

    SECTION("shutdown can be called multiple times") {

        log::init(temp_dir.path());

        log::shutdown();
        REQUIRE_NOTHROW(log::shutdown());

    }

    SECTION("shutdown before init does not throw") {

        REQUIRE_NOTHROW(log::shutdown());

    }

}

TEST_CASE("Logger | Logger creation", "[logger]") {

    const TempDir temp_dir;

    SECTION("creating a logger before logging system initialization throws") {

        REQUIRE_THROWS(log::get("test"));

    }

    SECTION("creating a logger after logging system initialization return valid logger") {

        log::init(temp_dir.path());
        
        auto test_logger = log::get("test");
        REQUIRE(test_logger);
        
        log::shutdown();

    }

    SECTION("creating a logger with an existing name returns a pointer to the same logger") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");
        auto another_test_logger = log::get("test");

        REQUIRE(test_logger.get() == another_test_logger.get());

        log::shutdown();

    }

    SECTION("creating two loggers with two different names return pointers to two different loggers") {

        log::init(temp_dir.path());

        auto test_logger_a = log::get("test_a");
        auto test_logger_b = log::get("test_b");

        REQUIRE_FALSE(test_logger_a.get() == test_logger_b.get());

        log::shutdown();

    }

    SECTION("creating a logger with an empty name throws") {

        log::init(temp_dir.path());
        REQUIRE_THROWS_AS(log::get(""),std::invalid_argument);
        log::shutdown();
    }

}

TEST_CASE("Logger | Log level", "[logger]") {

    const TempDir temp_dir;

    SECTION("on creation, loggers have the trace log level") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        auto lvl = test_logger->get_level();

        REQUIRE(lvl == log::LogLevel::trace);

        log::shutdown();

    }

    SECTION("changing the log level of a logger works") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        auto initial_lvl = test_logger->get_level();
        CHECK(initial_lvl == log::LogLevel::trace);

        test_logger->set_level(log::LogLevel::warn);
        auto new_lvl = test_logger->get_level();

        REQUIRE(new_lvl == log::LogLevel::warn);

        log::shutdown();

    }

    SECTION("changing the global log level works (all existing loggers get new level)") {

        log::init(temp_dir.path());

        auto test_logger_a = log::get("test_a");
        auto test_logger_b = log::get("test_b");
        test_logger_b->set_level(log::LogLevel::info);

        auto initial_lvl_a = test_logger_a->get_level();
        auto initial_lvl_b = test_logger_b->get_level();
        CHECK(initial_lvl_a == log::LogLevel::trace);
        CHECK(initial_lvl_b == log::LogLevel::info);

        log::set_global_log_level(log::LogLevel::warn);

        auto new_lvl_a = test_logger_a->get_level();
        auto new_lvl_b = test_logger_b->get_level();

        REQUIRE(new_lvl_a == log::LogLevel::warn);
        REQUIRE(new_lvl_b == log::LogLevel::warn);

        log::shutdown();

    }

    SECTION("all log level values roundtrip correctly") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        for (auto lvl : {log::LogLevel::trace, log::LogLevel::debug, log::LogLevel::info, log::LogLevel::warn, log::LogLevel::error, log::LogLevel::critical, log::LogLevel::off}) {
            test_logger->set_level(lvl);
            CHECK(test_logger->get_level() == lvl);
        }

        SUCCEED();

        log::shutdown();

    }

    SECTION("two loggers can have different log levels") {
        log::init(temp_dir.path());

        auto test_logger_a = log::get("test_a");
        auto test_logger_b = log::get("test_b");

        test_logger_a->set_level(log::LogLevel::info);
        test_logger_b->set_level(log::LogLevel::error);

        auto lvl_a = test_logger_a->get_level();
        auto lvl_b = test_logger_b->get_level();

        REQUIRE(lvl_a == log::LogLevel::info);
        REQUIRE(lvl_b == log::LogLevel::error);

        log::shutdown();
    }

    SECTION("newly created loggers have default trace level even after changing an existing logger's level") {
        log::init(temp_dir.path());

        auto test_logger_a = log::get("test_a");
        test_logger_a->set_level(log::LogLevel::error);

        auto lvl_a = test_logger_a->get_level();
        CHECK(lvl_a == log::LogLevel::error);

        auto test_logger_b = log::get("test_b");
        auto lvl_b = test_logger_b->get_level();

        REQUIRE(lvl_b == log::LogLevel::trace);

        log::shutdown();
    }
}

TEST_CASE("Logger | Log output", "[logger]") {

    const TempDir temp_dir;

    SECTION("info writes to logfile") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        test_logger->info("hello");
        test_logger->flush();

        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        std::string line;
        bool found_hello = false;
        while (std::getline(file, line)) {
            if (line.find("hello") != std::string::npos) {
                found_hello = true;
                break;
            }
        }
        
        REQUIRE(found_hello);

        log::shutdown();
    }

    SECTION("formatted messages work") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        test_logger->info("hello, this is a value: {}",21);
        test_logger->flush();

        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        std::string line;
        bool found_string = false;
        while (std::getline(file, line)) {
            if (line.find("hello, this is a value: 21") != std::string::npos) {
                found_string = true;
                break;
            }
        }

        REQUIRE(found_string);

        log::shutdown();
    }
    
    SECTION("all log levels output messages when log level allows") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");
        test_logger->set_level(log::LogLevel::trace);

        // Define test data for each log level
        struct LogLevelTest {
            log::LogLevel level;
            std::string message;
            std::function<void(std::shared_ptr<log::Logger> &, const std::string &)> log_func;
        };

        std::vector<LogLevelTest> test_cases = {
            {
                log::LogLevel::trace, "trace message",
                [](auto &logger, const auto &msg) { logger->trace("{}", msg); }
            },
            {
                log::LogLevel::debug, "debug message",
                [](auto &logger, const auto &msg) { logger->debug("{}", msg); }
            },
            {
                log::LogLevel::info, "info message",
                [](auto &logger, const auto &msg) { logger->info("{}", msg); }
            },
            {
                log::LogLevel::warn, "warn message",
                [](auto &logger, const auto &msg) { logger->warn("{}", msg); }
            },
            {
                log::LogLevel::error, "error message",
                [](auto &logger, const auto &msg) { logger->error("{}", msg); }
            },
            {
                log::LogLevel::critical, "critical message",
                [](auto &logger, const auto &msg) { logger->critical("{}", msg); }
            }
        };

        // Log a message for each level
        for (const auto &test_case: test_cases) {
            test_case.log_func(test_logger, test_case.message);
        }
        test_logger->flush();

        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        // Verify that each log level message appears in the file
        for (const auto &test_case: test_cases) {
            file.clear();
            file.seekg(0);

            std::string line;
            bool found_string = false;
            while (std::getline(file, line)) {
                if (line.find(test_case.message) != std::string::npos) {
                    found_string = true;
                    break;
                }
            }

            REQUIRE(found_string);
        }

        log::shutdown();
    }

}

TEST_CASE("Logger | Level filtering", "[logger]") {

    const TempDir temp_dir;

    SECTION("messages below a certain level are filtered out") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        test_logger->set_level(log::LogLevel::warn);

        test_logger->info("hello");
        test_logger->flush();

        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        std::string line;
        bool found_hello = false;
        while (std::getline(file, line)) {
            if (line.find("hello") != std::string::npos) {
                found_hello = true;
                break;
            }
        }

        REQUIRE_FALSE(found_hello);

        log::shutdown();

    }

    SECTION("messages at or above a certain level are not filtered out") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        test_logger->set_level(log::LogLevel::warn);

        test_logger->warn("hello");
        test_logger->flush();

        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        std::string line;
        bool found_hello = false;
        while (std::getline(file, line)) {
            if (line.find("hello") != std::string::npos) {
                found_hello = true;
                break;
            }
        }

        REQUIRE(found_hello);

        log::shutdown();

    }

    SECTION("LogLevel::off suppresses all output") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        test_logger->set_level(log::LogLevel::off);

        test_logger->trace("hello");
        test_logger->debug("hello");
        test_logger->info("hello");
        test_logger->warn("hello");
        test_logger->error("hello");
        test_logger->critical("hello");
        test_logger->flush();

        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        std::string line;
        bool found_hello = false;
        while (std::getline(file, line)) {
            if (line.find("hello") != std::string::npos) {
                found_hello = true;
                break;
            }
        }

        REQUIRE_FALSE(found_hello);

        log::shutdown();

    }

    SECTION("LogLevel::trace shows all messages") {

        log::init(temp_dir.path());

        auto test_logger = log::get("test");

        test_logger->set_level(log::LogLevel::trace);

        test_logger->trace("hello");
        test_logger->debug("hello");
        test_logger->info("hello");
        test_logger->warn("hello");
        test_logger->error("hello");
        test_logger->critical("hello");
        test_logger->flush();

        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        std::string line;
        int found_hello = 0;
        while (std::getline(file, line)) {
            if (line.find("hello") != std::string::npos) {
                found_hello++;
            }
        }

        REQUIRE(found_hello == 6);

        log::shutdown();

    }

}

TEST_CASE("Logger | Edge cases ", "[logger]") {

    const TempDir temp_dir;

    SECTION("Logger methods do nothing or throw if impl_ is null") {
        log::init(temp_dir.path());
        auto logger_ptr = log::get("test");
        
        // Move the content out of the Logger object pointed to by logger_ptr
        log::Logger logger_obj = std::move(*logger_ptr);
        log::Logger logger_obj2 = std::move(logger_obj);
        
        // Use move assignment to cover Logger& operator=(Logger&&)
        log::Logger logger_obj3(nullptr);
        logger_obj3 = std::move(logger_obj2);

        // Now logger_obj should have null impl_
        REQUIRE_THROWS_AS(logger_obj.get_level(), std::runtime_error);
        
        // These should not crash
        REQUIRE_NOTHROW(logger_obj.info("test"));
        REQUIRE_NOTHROW(logger_obj.set_level(log::LogLevel::info));
        REQUIRE_NOTHROW(logger_obj.flush());
        
        log::shutdown();
    }

    SECTION("log::get returns existing logger from spdlog registry if not in cache") {
        log::init(temp_dir.path());
        
        {
            auto spd_logger = std::make_shared<spdlog::logger>("manual_logger");
            spdlog::register_logger(spd_logger);
            
            // log::get("manual_logger") should find it in the spdlog registry
            auto logger = log::get("manual_logger");
            REQUIRE(logger != nullptr);
            
            // Clean up
            spdlog::drop("manual_logger");
        }
        
        log::shutdown();
    }

    SECTION("set_global_log_level covers all branches") {
        log::init(temp_dir.path());

        // Create a logger once at the beginning (at trace level by design)
        auto test_logger = log::get("test_global_logger");
        CHECK(test_logger->get_level() == log::LogLevel::trace);

        // Iterate over each global log level
        for (auto lvl: {
                 log::LogLevel::trace, log::LogLevel::debug, log::LogLevel::info,
                 log::LogLevel::warn, log::LogLevel::error, log::LogLevel::critical,
                 log::LogLevel::off
             }) {
            // Set the global level
            log::set_global_log_level(lvl);

            // Verify that the existing logger's level follows the global level change
            CHECK(test_logger->get_level() == lvl);
        }
    
        log::shutdown();
    }

    SECTION("string_to_log_level converts all valid level strings") {
        REQUIRE(log::string_to_log_level("trace") == log::LogLevel::trace);
        REQUIRE(log::string_to_log_level("debug") == log::LogLevel::debug);
        REQUIRE(log::string_to_log_level("info") == log::LogLevel::info);
        REQUIRE(log::string_to_log_level("warn") == log::LogLevel::warn);
        REQUIRE(log::string_to_log_level("error") == log::LogLevel::error);
        REQUIRE(log::string_to_log_level("critical") == log::LogLevel::critical);
        REQUIRE(log::string_to_log_level("off") == log::LogLevel::off);
    }

    SECTION("string_to_log_level throws for invalid string") {
        REQUIRE_THROWS_AS(log::string_to_log_level("invalid"), std::invalid_argument);
        REQUIRE_THROWS_AS(log::string_to_log_level(""), std::invalid_argument);
        REQUIRE_THROWS_AS(log::string_to_log_level("INFO"), std::invalid_argument);
        REQUIRE_THROWS_AS(log::string_to_log_level("Warning"), std::invalid_argument);
    }

    SECTION("get_level covers all spdlog level cases") {
        log::init(temp_dir.path());

        auto test_logger = log::get("test_level_coverage");

        // Test that each level roundtrips correctly through get_level
        for (auto lvl : {
            log::LogLevel::trace,
            log::LogLevel::debug, 
            log::LogLevel::info,
            log::LogLevel::warn,
            log::LogLevel::error,
            log::LogLevel::critical,
            log::LogLevel::off
        }) {
            test_logger->set_level(lvl);
            REQUIRE(test_logger->get_level() == lvl);
        }

        log::shutdown();
    }

    SECTION("init with empty path uses default log directory") {
        // This covers the empty path branch in init
        log::init(fs::path{});

        REQUIRE(log::is_initialized());

        auto test_logger = log::get("test_default_path");
        REQUIRE(test_logger != nullptr);

        log::shutdown();
    }

    SECTION("multiple calls to init with different paths") {
        log::init(temp_dir.path());
        
        const auto initial_state = log::is_initialized();
        REQUIRE(initial_state);

        // Call init again with a different path - should return early
        const TempDir temp_dir2;
        log::init(temp_dir2.path());

        // Should still be initialized
        REQUIRE(log::is_initialized());

        log::shutdown();
    }

    SECTION("logger caching works correctly") {
        log::init(temp_dir.path());

        // First call creates and caches the logger
        auto logger1 = log::get("cached_logger");
        REQUIRE(logger1 != nullptr);

        // Second call returns cached logger (hits the cache branch)
        auto logger2 = log::get("cached_logger");
        REQUIRE(logger2 != nullptr);
        REQUIRE(logger1.get() == logger2.get());

        log::shutdown();
    }

    SECTION("logger pattern is set correctly on creation") {
        log::shutdown(); // Ensure clean state
        
        log::init(temp_dir.path());

        auto test_logger = log::get("pattern_test");
        test_logger->info("test message with pattern");
        test_logger->flush();
        
        // Force immediate flush by dropping the logger
        spdlog::drop("pattern_test");
        
        // Longer delay to ensure file is written
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Verify the log file has the expected pattern
        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        std::string line;
        bool found_pattern = false;
        const std::regex expected_line{
            R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} \[[^\]]+\] \[[^\]]+\] test message with pattern$)"};
        while (std::getline(file, line)) {
            // Logger name is formatted with width (%10!n), so don't match it literally.
            if (std::regex_match(line, expected_line)) {
                found_pattern = true;
                break;
            }
        }

        REQUIRE(found_pattern);

        log::shutdown();
    }

    SECTION("unique session filename generation when file exists") {
        log::shutdown(); // Ensure clean state
        
        // Use a separate temp dir for this test to avoid interference
        const TempDir temp_dir_unique;
        
        log::init(temp_dir_unique.path());

        // Create first logger to generate a log file
        auto logger1 = log::get("test1");
        logger1->info("first message");
        logger1->flush();
        spdlog::drop("test1");

        log::shutdown();
        
        // Sleep to ensure different timestamp in filename
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Initialize again - should create a new file with unique name
        log::init(temp_dir_unique.path());

        auto logger2 = log::get("test2");
        logger2->info("second message");
        logger2->flush();
        spdlog::drop("test2");
        
        // Longer delay to ensure files are written
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Count log files
        int log_file_count = 0;
        for (const auto &entry: fs::directory_iterator(temp_dir_unique.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                log_file_count++;
            }
        }

        // Should have at least 2 log files
        REQUIRE(log_file_count >= 2);

        log::shutdown();
    }

    SECTION("logger sinks are shared across loggers") {
        log::shutdown(); // Ensure clean state
        
        // Use a separate temp dir for this test
        const TempDir temp_dir_shared;
        
        log::init(temp_dir_shared.path());

        auto logger1 = log::get("logger1");
        auto logger2 = log::get("logger2");

        logger1->info("from logger1");
        logger2->info("from logger2");
        logger1->flush();
        logger2->flush();
        
        // Drop loggers to force flush
        spdlog::drop("logger1");
        spdlog::drop("logger2");
        
        // Longer delay to ensure files are written
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Both messages should appear in the same log file
        bool found_log_file = false;
        std::string log_file_path_str;
        for (const auto &entry: fs::directory_iterator(temp_dir_shared.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                found_log_file = true;
                log_file_path_str = entry.path().string();
                break;
            }
        }
        REQUIRE(found_log_file);

        std::ifstream file(log_file_path_str);
        REQUIRE(file.is_open());

        bool found_logger1 = false;
        bool found_logger2 = false;
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("from logger1") != std::string::npos) {
                found_logger1 = true;
            }
            if (line.find("from logger2") != std::string::npos) {
                found_logger2 = true;
            }
        }

        REQUIRE(found_logger1);
        REQUIRE(found_logger2);

        log::shutdown();
    }

    SECTION("console sink is created and configured") {
        log::init(temp_dir.path());

        // Create logger and log at trace level
        auto test_logger = log::get("console_test");
        test_logger->set_level(log::LogLevel::trace);
        
        // Log messages at all levels to ensure console sink is working
        test_logger->trace("trace to console");
        test_logger->debug("debug to console");
        test_logger->info("info to console");
        test_logger->warn("warn to console");
        test_logger->error("error to console");
        test_logger->critical("critical to console");
        test_logger->flush();

        // If we got here without crashes, console sink is working
        SUCCEED();

        log::shutdown();
    }

    SECTION("file sink rotation settings are applied") {
        log::init(temp_dir.path());

        auto test_logger = log::get("rotation_test");

        // Write enough data to potentially trigger rotation (though 5MB is large)
        for (int i = 0; i < 1000; ++i) {
            test_logger->info("This is log message number {} with some padding to increase size", i);
        }
        test_logger->flush();

        // At minimum, one log file should exist
        int log_file_count = 0;
        for (const auto &entry: fs::directory_iterator(temp_dir.path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                log_file_count++;
            }
        }

        REQUIRE(log_file_count >= 1);

        log::shutdown();
    }

    SECTION("is_initialized returns false before init") {
        // Make sure logging is shut down first
        log::shutdown();

        REQUIRE_FALSE(log::is_initialized());

        // Re-initialize for other tests
        log::init(temp_dir.path());
        REQUIRE(log::is_initialized());
        log::shutdown();
    }

    SECTION("shutdown clears sinks and loggers") {
        log::init(temp_dir.path());

        auto logger = log::get("test_shutdown");
        REQUIRE(log::is_initialized());

        log::shutdown();

        REQUIRE_FALSE(log::is_initialized());

        // After shutdown, attempting to get a logger should throw
        REQUIRE_THROWS_AS(log::get("another_logger"), std::runtime_error);
    }
}



