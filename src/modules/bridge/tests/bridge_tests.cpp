//
// Created by Nicolas Désilles on 14/01/2026.
//
// Note for future self: I used some AI to make most of these tests to increase dev speed.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <thread>
#include <chrono>
#include <set>
#include <atomic>

#include <logger.h>
#include <settings.h>

#include "bridge.h"
#include "startup.h"
#include "startup_step.h"
#include "startup_manager.h"

using namespace aknet;

namespace fs = std::filesystem;

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

class TempDir {
    fs::path path_;
public:
    TempDir() : path_(fs::temp_directory_path() / "aknet_bridge_tests") {
        fs::create_directories(path_);
    }
    ~TempDir() {
        try {
            fs::remove_all(path_);
        } catch (...) {}
    }
    const fs::path& path() const { return path_; }
};

class MockWebview {
public:
    std::vector<std::string> executed_js;
    mutable std::mutex mutex;

    void execute(const std::string& js) {
        std::lock_guard lock(mutex);
        executed_js.push_back(js);
    }

    size_t call_count() const {
        std::lock_guard lock(mutex);
        return executed_js.size();
    }

    std::string get_call(size_t index) const {
        std::lock_guard lock(mutex);
        return (index < executed_js.size()) ? executed_js[index] : "";
    }

    bool has_call_containing(const std::string& needle) const {
        std::lock_guard lock(mutex);
        for (const auto& js : executed_js) {
            if (js.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    void clear() {
        std::lock_guard lock(mutex);
        executed_js.clear();
    }
};

class FakeStep : public startup::IStartupStep {
    startup::StepConfig config_;
    startup::StepResult result_;

public:
    FakeStep(startup::StepConfig cfg, startup::StepResult res)
        : config_(std::move(cfg)), result_(std::move(res)) {}

    const startup::StepConfig& config() const override {
        return config_;
    }

    startup::StepResult run(startup::StepContext&) override {
        return result_;
    }
};

struct BridgeTestFixture {
    std::shared_ptr<log::Logger> logger;
    std::shared_ptr<settings::Settings> settings;
    std::shared_ptr<MockWebview> webview;
    TempDir temp_dir;

    BridgeTestFixture() {
        log::init(temp_dir.path());
        log::set_global_log_level(log::LogLevel::debug);

        logger = log::get("bridge_test");

        settings = std::make_shared<settings::Settings>();
        auto settings_config = settings::SettingsConfig{
            .base_dir = temp_dir.path(),
            .schema_version = 1
        };
        auto settings_logger = log::get("settings");
        settings->init(settings_logger, settings_config);
        settings->load_or_create();

        webview = std::make_shared<MockWebview>();
    }

    ~BridgeTestFixture() {
        settings->shutdown();
        log::shutdown();
    }

    std::shared_ptr<startup::StartupManager> make_manager() {
        auto startup_logger = log::get("startup");
        auto clock = std::make_shared<startup::SteadyClock>();
        return std::make_shared<startup::StartupManager>(startup_logger, settings, clock);
    }
};

// ------------------------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("EventBridge | Construction", "[bridge]") {

    SECTION("constructor accepts valid parameters") {
        BridgeTestFixture f;

        REQUIRE_NOTHROW(bridge::EventBridge(f.logger, f.webview.get()));
    }

    SECTION("constructor throws when logger is null") {
        BridgeTestFixture f;

        REQUIRE_THROWS_AS(
            bridge::EventBridge(nullptr, f.webview.get()),
            std::invalid_argument
        );
    }

    SECTION("constructor throws when webview is null") {
        BridgeTestFixture f;
        MockWebview* null_webview = nullptr;

        REQUIRE_THROWS_AS(
            bridge::EventBridge(f.logger, null_webview),
            std::invalid_argument
        );
    }

}

TEST_CASE("EventBridge | connect_startup_events validation", "[bridge]") {

    SECTION("connect_startup_events throws when manager is null") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());

        REQUIRE_THROWS_AS(
            bridge.connect_startup_events(nullptr),
            std::invalid_argument
        );
    }

    SECTION("connect_startup_events accepts valid manager") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        REQUIRE_NOTHROW(bridge.connect_startup_events(manager));
    }

}

TEST_CASE("EventBridge | StartupManager event forwarding", "[bridge]") {

    SECTION("ProgressChanged event is forwarded to webview") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->call_count() > 0);
        REQUIRE(f.webview->has_call_containing("startup:progress"));
    }

    SECTION("StateChanged event is forwarded to webview") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->has_call_containing("startup:state"));
    }

    SECTION("StepStarted event is forwarded with step id") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "my_test_step", .display_name = "My Test Step", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->has_call_containing("startup:step_started"));
        REQUIRE(f.webview->has_call_containing("my_test_step"));
    }

    SECTION("StepCompleted event is forwarded to webview") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->has_call_containing("startup:step_completed"));
    }

    SECTION("SequenceCompleted event is forwarded with success flag") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->has_call_containing("startup:completed"));
        REQUIRE(f.webview->has_call_containing("\"success\":true"));
    }

    SECTION("Error event is forwarded when critical step fails") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{
                .id = "failing_step",
                .display_name = "Failing Step",
                .timeout = std::chrono::seconds{5},
                .critical = true
            },
            startup::StepResult{startup::StepStatus::Failed, "Something went wrong"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->has_call_containing("startup:error"));
    }

    SECTION("all event types fire during successful sequence") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        std::set<std::string> event_types;
        for (size_t i = 0; i < f.webview->call_count(); ++i) {
            const auto& js = f.webview->get_call(i);
            if (js.find("startup:progress") != std::string::npos)
                event_types.insert("progress");
            if (js.find("startup:state") != std::string::npos)
                event_types.insert("state");
            if (js.find("startup:step_started") != std::string::npos)
                event_types.insert("step_started");
            if (js.find("startup:step_completed") != std::string::npos)
                event_types.insert("step_completed");
            if (js.find("startup:completed") != std::string::npos)
                event_types.insert("completed");
        }

        REQUIRE(event_types.count("progress") == 1);
        REQUIRE(event_types.count("state") == 1);
        REQUIRE(event_types.count("step_started") == 1);
        REQUIRE(event_types.count("step_completed") == 1);
        REQUIRE(event_types.count("completed") == 1);
    }

}

TEST_CASE("EventBridge | disconnect", "[bridge]") {

    SECTION("disconnect before connect is safe") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());

        REQUIRE_NOTHROW(bridge.disconnect());
    }

    SECTION("disconnect stops event forwarding") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);
        bridge.disconnect();

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->call_count() == 0);
    }

    SECTION("multiple disconnect calls are safe") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);

        REQUIRE_NOTHROW(bridge.disconnect());
        REQUIRE_NOTHROW(bridge.disconnect());
        REQUIRE_NOTHROW(bridge.disconnect());
    }

    SECTION("can reconnect after disconnect") {
        BridgeTestFixture f;
        bridge::EventBridge bridge(f.logger, f.webview.get());
        auto manager = f.make_manager();

        bridge.connect_startup_events(manager);
        bridge.disconnect();

        REQUIRE_NOTHROW(bridge.connect_startup_events(manager));

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->call_count() > 0);
    }

}

TEST_CASE("EventBridge | Destructor cleanup", "[bridge]") {

    SECTION("destructor disconnects from events") {
        BridgeTestFixture f;
        auto manager = f.make_manager();

        {
            bridge::EventBridge bridge(f.logger, f.webview.get());
            bridge.connect_startup_events(manager);
        }

        manager->add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager->start_async();

        int attempts = 0;
        while (manager->is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE(f.webview->call_count() == 0);
    }

}


