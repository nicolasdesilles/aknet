//
// Created by Nicolas Désilles on 23/01/2026.
//

#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <chrono>
#include <vector>
#include <memory>
#include <filesystem>
#include <thread>

#include <logger.h>
#include <settings.h>

#include "startup.h"
#include "clock.h"
#include "startup_step.h"
#include "startup_engine.h"
#include "startup_manager.h"
#include "startup_json.h"

namespace aknet::test {

namespace fs = std::filesystem;

// ------------------------------------------------------------------------------------------------
// Test Helpers
// ------------------------------------------------------------------------------------------------

// Helper to create a temporary test directory
class TempDir {
    fs::path path_;
public:
    TempDir() {
        path_ = fs::temp_directory_path() / ("aknet_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(path_);
    }

    ~TempDir() {
        if (fs::exists(path_)) {
            fs::remove_all(path_);
        }
    }

    fs::path path() const { return path_; }
};

// To avoid using sleep()
struct FakeClock : startup::IClock {
    std::chrono::steady_clock::time_point t{};

    std::chrono::steady_clock::time_point now() const override {
        return t;
    }

    void advance(std::chrono::seconds seconds) {
        t += seconds;
    }
};

// A step that basically does nothing and returns a pre-defined result
class FakeStep : public startup::IStartupStep {
    startup::StepConfig config_;
    startup::StepResult result_;

public:
    FakeStep(startup::StepConfig cfg, startup::StepResult res = {startup::StepStatus::Success, "OK"})
        : config_(std::move(cfg)), result_(std::move(res)) {}

    const startup::StepConfig& config() const override {
        return config_;
    }

    startup::StepResult run(startup::StepContext& ctx) override {
        return result_;
    }
};

// A step that takes real time to execute (for abort testing)
class SlowRealStep : public startup::IStartupStep {
    startup::StepConfig config_;
public:
    explicit SlowRealStep(startup::StepConfig cfg) : config_(std::move(cfg)) {}

    const startup::StepConfig& config() const override { return config_; }

    startup::StepResult run(startup::StepContext& ctx) override {
        // Sleep in small increments to allow abort checking
        for (int i = 0; i < 15; ++i) {
            if (ctx.abort_requested()) {
                return {startup::StepStatus::Aborted, "Aborted"};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return {startup::StepStatus::Success, "Completed"};
    }
};

// A step that simulates slow execution by advancing the clock
class SlowStep : public startup::IStartupStep {
    startup::StepConfig config_;
    std::shared_ptr<FakeClock> clock_;
    std::chrono::seconds execution_time_;
    startup::StepResult result_;

public:
    SlowStep(startup::StepConfig cfg,
             std::shared_ptr<FakeClock> clk,
             std::chrono::seconds exec_time,
             startup::StepResult res = {startup::StepStatus::Success, "OK"})
        : config_(std::move(cfg)),
          clock_(std::move(clk)),
          execution_time_(exec_time),
          result_(std::move(res)) {}

    const startup::StepConfig& config() const override {
        return config_;
    }

    startup::StepResult run(startup::StepContext& ctx) override {
        clock_->advance(execution_time_);
        return result_;
    }
};

struct EngineTestFixture {
    std::shared_ptr<log::Logger> logger_settings;
    std::shared_ptr<log::Logger> logger_startup;
    std::shared_ptr<settings::Settings> settings;
    std::shared_ptr<FakeClock> clock;
    TempDir temp_dir;

    EngineTestFixture() {
        log::init(temp_dir.path());
        logger_settings = log::get("settings");
        logger_startup = log::get("startup_t");

        settings = std::make_shared<settings::Settings>();
        settings->init(logger_settings, settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .schema_version = 1
        });

        clock = std::make_shared<FakeClock>();
    }

    ~EngineTestFixture() {
        settings->shutdown();
        log::shutdown();
    }
};

} // namespace aknet::test
