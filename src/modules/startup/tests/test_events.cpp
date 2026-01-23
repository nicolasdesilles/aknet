//
// Created by Nicolas Désilles on 23/01/2026.
//

#include "test_fixtures.h"
#include <mutex>

using namespace aknet;
using namespace aknet::test;

// ------------------------------------------------------------------------------------------------
// Event System Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Startup | Events - Basic subscription works", "[startup][events]") {

    // Setup
    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("events() returns valid event manager") {
        auto& events = manager.events();
        REQUIRE_NOTHROW(events.get<startup::StartupManager::Event::ProgressChanged>());
    }

    SECTION("can subscribe to ProgressChanged event") {
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&event_fired](const startup::SequenceProgress&) {
                event_fired = true;
            });

        // Event hasn't fired yet
        REQUIRE_FALSE(event_fired);
    }

    SECTION("can subscribe to StateChanged event") {
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&event_fired](startup::AppState, startup::AppState) {
                event_fired = true;
            });

        REQUIRE_FALSE(event_fired);
    }

    SECTION("can subscribe to multiple events") {
        int progress_count = 0;
        int state_count = 0;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&progress_count](const startup::SequenceProgress&) {
                progress_count++;
            });

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&state_count](startup::AppState, startup::AppState) {
                state_count++;
            });

        REQUIRE(progress_count == 0);
        REQUIRE(state_count == 0);
    }
}

TEST_CASE("Startup | Events - StateChanged fires on state transitions", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("StateChanged fires when sequence starts (off to booting)") {
        std::vector<std::pair<startup::AppState, startup::AppState>> state_changes;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
        .add([&](startup::AppState old_s, startup::AppState new_s) {
            state_changes.push_back({old_s, new_s});
        });

        // Setup a simple successful step
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait for at least one event
        int attempts = 0;
        while (state_changes.empty() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        REQUIRE_FALSE(state_changes.empty());
        REQUIRE(state_changes[0].first == startup::AppState::Off);
        REQUIRE(state_changes[0].second == startup::AppState::Booting);
    }

    SECTION("StateChanged fires when sequence completes (booting to active)") {
        std::vector<std::pair<startup::AppState, startup::AppState>> state_changes;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                state_changes.push_back({old_s, new_s});
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        // Give events time to fire
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have two state changes: Off to Booting, Booting to Active
        REQUIRE(state_changes.size() >= 2);
        REQUIRE(state_changes[0].first == startup::AppState::Off);
        REQUIRE(state_changes[0].second == startup::AppState::Booting);
        REQUIRE(state_changes[1].first == startup::AppState::Booting);
        REQUIRE(state_changes[1].second == startup::AppState::Active);
    }

    SECTION("StateChanged fires on failure (booting to off)") {
        std::vector<std::pair<startup::AppState, startup::AppState>> state_changes;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                state_changes.push_back({old_s, new_s});
            });

        // Critical step that fails
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}, .critical = true},
            startup::StepResult{startup::StepStatus::Failed, "Failed"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have two state changes: Off to Booting, Booting to Off
        REQUIRE(state_changes.size() >= 2);
        REQUIRE(state_changes[0].first == startup::AppState::Off);
        REQUIRE(state_changes[0].second == startup::AppState::Booting);
        REQUIRE(state_changes[1].first == startup::AppState::Booting);
        REQUIRE(state_changes[1].second == startup::AppState::Off);
    }
}

TEST_CASE("Startup | Events - ProgressChanged fires during execution", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("ProgressChanged fires at least once during sequence") {
        std::vector<startup::SequenceProgress> progress_snapshots;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress& progress) {
                progress_snapshots.push_back(progress);
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have received at least one progress update
        REQUIRE_FALSE(progress_snapshots.empty());
    }

    SECTION("ProgressChanged provides accurate progress data") {
        std::vector<startup::SequenceProgress> progress_snapshots;

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress& progress) {
                progress_snapshots.push_back(progress);
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Verify we got progress updates
        REQUIRE_FALSE(progress_snapshots.empty());

        // Last update should show completion
        const auto& final_progress = progress_snapshots.back();
        REQUIRE(final_progress.state == startup::AppState::Active);
        REQUIRE(final_progress.steps.size() == 2);
    }
}
TEST_CASE("Startup | Events - StepStarted and StepCompleted fire", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("StepStarted fires when step begins") {
        int step_index = -1;
        std::string step_id;
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int index, const std::string& id) {
                step_index = index;
                step_id = id;
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "test_step", .display_name = "Test", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(step_index == 0);
        REQUIRE(step_id == "test_step");
    }

    SECTION("StepCompleted fires when step finishes") {
        int step_index = -1;
        std::string step_id;
        startup::StepStatus status = startup::StepStatus::Pending;
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int index, const std::string& id, startup::StepStatus st) {
                step_index = index;
                step_id = id;
                status = st;
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "completed_step", .display_name = "Test", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "Done"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(step_index == 0);
        REQUIRE(step_id == "completed_step");
        REQUIRE(status == startup::StepStatus::Success);
    }

    SECTION("Both events fire in correct order for multiple steps") {
        std::vector<std::string> event_log;

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int index, const std::string& id) {
                event_log.push_back("START:" + id);
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int index, const std::string& id, startup::StepStatus) {
                event_log.push_back("COMPLETE:" + id);
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_log.size() == 4);
        REQUIRE(event_log[0] == "START:step1");
        REQUIRE(event_log[1] == "COMPLETE:step1");
        REQUIRE(event_log[2] == "START:step2");
        REQUIRE(event_log[3] == "COMPLETE:step2");
    }

    SECTION("StepCompleted reports Failed status correctly") {
        startup::StepStatus final_status = startup::StepStatus::Pending;

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int, const std::string&, startup::StepStatus st) {
                final_status = st;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "fail_step", .display_name = "Fail", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Failed, "Error"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(final_status == startup::StepStatus::Failed);
    }
}

TEST_CASE("Startup | Events - Error event fires on critical failures", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("error event fires when critical step fails") {
        std::string error_message;
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::Error>()
            .add([&](const std::string& msg) {
                error_message = msg;
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "critical_step", .display_name = "Critical", .timeout = std::chrono::seconds{5}, .critical = true},
            startup::StepResult{startup::StepStatus::Failed, "Database connection failed"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE_FALSE(error_message.empty());
        REQUIRE(error_message.find("critical_step") != std::string::npos);
    }

    SECTION("error event does not fire on non-critical failure") {
        bool event_fired = false;

        manager.events().get<startup::StartupManager::Event::Error>()
            .add([&](const std::string&) {
                event_fired = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "non_critical", .display_name = "Non-Critical", .timeout = std::chrono::seconds{5}, .critical = false},
            startup::StepResult{startup::StepStatus::Failed, "Optional service unavailable"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "next_step", .display_name = "Next", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should NOT fire error for non-critical failure
        REQUIRE_FALSE(event_fired);
    }
}

TEST_CASE("Startup | Events - Complete event flow integration", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("all events fire in correct order for successful sequence") {
        std::vector<std::string> event_log;

        // Subscribe to all events
        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                event_log.push_back("STATE:" + std::to_string(static_cast<int>(old_s)) + "->" + std::to_string(static_cast<int>(new_s)));
            });

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int index, const std::string& id) {
                event_log.push_back("STEP_START:" + std::to_string(index) + ":" + id);
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int index, const std::string& id, startup::StepStatus) {
                event_log.push_back("STEP_DONE:" + std::to_string(index) + ":" + id);
            });

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress&) {
                event_log.push_back("PROGRESS");
            });

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool success, const std::string&) {
                event_log.push_back("SEQUENCE:" + std::string(success ? "SUCCESS" : "FAIL"));
            });

        // Setup two-step sequence
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "s1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "s2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_log.size() >= 7);

        // First event should be state change Off to Booting
        REQUIRE(event_log[0] == "STATE:0->1");

        // Should have step events for both steps
        bool found_s1_start = false;
        bool found_s1_done = false;
        bool found_s2_start = false;
        bool found_s2_done = false;

        for (const auto& evt : event_log) {
            if (evt == "STEP_START:0:s1") found_s1_start = true;
            if (evt == "STEP_DONE:0:s1") found_s1_done = true;
            if (evt == "STEP_START:1:s2") found_s2_start = true;
            if (evt == "STEP_DONE:1:s2") found_s2_done = true;
        }

        REQUIRE(found_s1_start);
        REQUIRE(found_s1_done);
        REQUIRE(found_s2_start);
        REQUIRE(found_s2_done);

        // Last event should be SequenceCompleted with success
        REQUIRE(event_log.back() == "SEQUENCE:SUCCESS");
    }

    SECTION("Events fire in deterministic order") {
        std::vector<std::string> event_order;
        std::mutex log_mutex;  // Protect shared vector from race conditions

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("STATE");
            });

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int, const std::string&) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("STEP_START");
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int, const std::string&, startup::StepStatus) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("STEP_COMPLETE");
            });

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress&) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("PROGRESS");
            });

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool, const std::string&) {
                std::lock_guard lock(log_mutex);
                event_order.push_back("SEQUENCE");
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Verify order: STATE (Off→Booting), STEP_START, STEP_COMPLETE, PROGRESS, STATE (Booting→Active), SEQUENCE
        REQUIRE(event_order.size() >= 6);
        REQUIRE(event_order[0] == "STATE");  // Off→Booting
        REQUIRE(event_order[1] == "STEP_START");
        REQUIRE(event_order[2] == "STEP_COMPLETE");
        REQUIRE(event_order.back() == "SEQUENCE");
    }
}

TEST_CASE("Startup | Events - Multiple subscribers", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("Multiple subscribers all receive StateChanged events") {
        int subscriber1_count = 0;
        int subscriber2_count = 0;
        int subscriber3_count = 0;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                subscriber1_count++;
            });

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                subscriber2_count++;
            });

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                subscriber3_count++;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // All three subscribers should have received the same number of events
        REQUIRE(subscriber1_count >= 2);  // At least Off→Booting and Booting→Active
        REQUIRE(subscriber1_count == subscriber2_count);
        REQUIRE(subscriber2_count == subscriber3_count);
    }

    SECTION("Multiple subscribers on different event types all receive events") {
        bool state_received = false;
        bool progress_received = false;
        bool step_started_received = false;
        bool step_completed_received = false;
        bool sequence_completed_received = false;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                state_received = true;
            });

        manager.events().get<startup::StartupManager::Event::ProgressChanged>()
            .add([&](const startup::SequenceProgress&) {
                progress_received = true;
            });

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int, const std::string&) {
                step_started_received = true;
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int, const std::string&, startup::StepStatus) {
                step_completed_received = true;
            });

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool, const std::string&) {
                sequence_completed_received = true;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(state_received);
        REQUIRE(progress_received);
        REQUIRE(step_started_received);
        REQUIRE(step_completed_received);
        REQUIRE(sequence_completed_received);
    }
}

TEST_CASE("Startup | Events - Unsubscribe", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("Unsubscribed handler does not receive events") {
        int active_count = 0;
        int removed_count = 0;

        // This subscriber will remain active
        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                active_count++;
            });

        // This subscriber will be removed before the sequence runs
        auto subscription_id = manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                removed_count++;
            });

        // Remove the second subscriber BEFORE running
        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .remove(subscription_id);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(active_count >= 2);  // Active subscriber received events
        REQUIRE(removed_count == 0);  // Removed subscriber received NOTHING
    }

    SECTION("clear() removes all subscribers") {
        int count = 0;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                count++;
            });

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState, startup::AppState) {
                count++;
            });

        // Clear all subscribers
        manager.events().get<startup::StartupManager::Event::StateChanged>().clear();

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(count == 0);  // No events received after clear()
    }
}

TEST_CASE("Startup | Events - Abort behavior", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("Aborted steps do not fire StepStarted/StepCompleted") {
        std::vector<std::string> event_log;

        manager.events().get<startup::StartupManager::Event::StepStarted>()
            .add([&](int index, const std::string& id) {
                event_log.push_back("START:" + id);
            });

        manager.events().get<startup::StartupManager::Event::StepCompleted>()
            .add([&](int index, const std::string& id, startup::StepStatus status) {
                event_log.push_back("COMPLETE:" + id + ":" + std::to_string(static_cast<int>(status)));
            });

        // Use a slow step so we have time to abort
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow_step", .display_name = "Slow Step", .timeout = std::chrono::seconds{10}}
        ));
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "never_reached", .display_name = "Never Reached", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit then abort
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        manager.request_abort();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have started slow_step
        bool slow_started = false;
        bool never_reached_started = false;

        for (const auto& evt : event_log) {
            if (evt.find("START:slow_step") != std::string::npos) slow_started = true;
            if (evt.find("START:never_reached") != std::string::npos) never_reached_started = true;
        }

        REQUIRE(slow_started);
        REQUIRE_FALSE(never_reached_started);  // This step should never have started
    }

    SECTION("StateChanged fires correctly when aborted (Booting to Off)") {
        std::vector<std::pair<startup::AppState, startup::AppState>> state_changes;

        manager.events().get<startup::StartupManager::Event::StateChanged>()
            .add([&](startup::AppState old_s, startup::AppState new_s) {
                state_changes.push_back({old_s, new_s});
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow_step", .display_name = "Slow Step", .timeout = std::chrono::seconds{10}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit then abort
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        manager.request_abort();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should have: Off→Booting, then Booting→Off (due to abort)
        REQUIRE(state_changes.size() >= 2);
        REQUIRE(state_changes[0].first == startup::AppState::Off);
        REQUIRE(state_changes[0].second == startup::AppState::Booting);
        REQUIRE(state_changes[1].first == startup::AppState::Booting);
        REQUIRE(state_changes[1].second == startup::AppState::Off);
    }

    SECTION("SequenceCompleted fires with success=false when aborted") {
        bool sequence_completed_fired = false;
        bool success_value = true;  // Start with true to verify it changes
        std::string error_msg;

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool success, const std::string& msg) {
                sequence_completed_fired = true;
                success_value = success;
                error_msg = msg;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow_step", .display_name = "Slow Step", .timeout = std::chrono::seconds{10}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit then abort
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        manager.request_abort();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(sequence_completed_fired);
        REQUIRE(success_value == false);  // Abort means failure
    }
}

TEST_CASE("Startup | Events - SequenceCompleted", "[startup][events]") {

    EngineTestFixture f;
    auto real_clock = std::make_shared<startup::SteadyClock>();
    startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

    SECTION("SequenceCompleted fires with success=true on successful completion") {
        bool event_fired = false;
        bool success = false;
        std::string error;

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool s, const std::string& e) {
                event_fired = true;
                success = s;
                error = e;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(success == true);
        REQUIRE(error.empty());
    }

    SECTION("SequenceCompleted fires with success=false and error message on failure") {
        bool event_fired = false;
        bool success = true;
        std::string error;

        manager.events().get<startup::StartupManager::Event::SequenceCompleted>()
            .add([&](bool s, const std::string& e) {
                event_fired = true;
                success = s;
                error = e;
            });

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "fail_step", .display_name = "Fail Step", .timeout = std::chrono::seconds{5}, .critical = true},
            startup::StepResult{startup::StepStatus::Failed, "Critical error"}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(event_fired);
        REQUIRE(success == false);
        REQUIRE_FALSE(error.empty());
    }
}