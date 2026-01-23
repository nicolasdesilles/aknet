//
// Created by Nicolas Désilles on 23/01/2026.
//

#include "test_fixtures.h"

using namespace aknet;
using namespace aknet::test;

// ------------------------------------------------------------------------------------------------
// StartupManager Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Startup | StartupManager - Basic async execution", "[startup][manager][async]") {

    SECTION("start_async runs sequence in background") {
        EngineTestFixture f;
        // Use real clock for async tests
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        auto set_result = manager.set_steps(std::move(steps));
        REQUIRE(set_result.ok);

        auto start_result = manager.start_async();
        REQUIRE(start_result.ok);

        // Wait for completion (poll with timeout)
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        // Also wait a bit more to ensure cache is updated
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        REQUIRE_FALSE(manager.is_running());

        auto progress = manager.get_progress();
        REQUIRE(progress.state == startup::AppState::Active);
        REQUIRE(progress.steps[0].status == startup::StepStatus::Success);
    }

    SECTION("start_async fails when already running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow", .display_name = "Slow", .timeout = std::chrono::seconds{5}}
        ));

        manager.set_steps(std::move(steps));
        auto result1 = manager.start_async();
        REQUIRE(result1.ok);

        // Wait a tiny bit to ensure thread has started
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Try to start again while running
        auto result2 = manager.start_async();
        REQUIRE_FALSE(result2.ok);
        REQUIRE(result2.error.find("already running") != std::string::npos);

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
    }

    SECTION("start_async fails with no steps") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        auto result = manager.start_async();
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("No steps") != std::string::npos);
    }

}

TEST_CASE("Startup | StartupManager - Abort during async execution", "[startup][manager][async][abort]") {

    SECTION("request_abort stops running sequence") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Create a step that checks abort periodically
        class AbortCheckingStep : public startup::IStartupStep {
            startup::StepConfig config_;
        public:
            explicit AbortCheckingStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext& ctx) override {
                for (int i = 0; i < 50; ++i) {
                    if (ctx.abort_requested()) {
                        return {startup::StepStatus::Aborted, "Aborted by request"};
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                return {startup::StepStatus::Success, "Completed"};
            }
        };

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<AbortCheckingStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{10}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit then abort
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        manager.request_abort(startup::AbortReason::UserRequested);

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 200) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        auto progress = manager.get_progress();
        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.abort_reason == startup::AbortReason::UserRequested);
    }

}

TEST_CASE("Startup | StartupManager - Retry async", "[startup][manager][async][retry]") {

    SECTION("retry_async re-runs failed sequence") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Step that fails first time, succeeds on retry
        class FailOnceThenSucceedStep : public startup::IStartupStep {
            startup::StepConfig config_;
            mutable std::atomic<int> call_count_{0};
        public:
            explicit FailOnceThenSucceedStep(startup::StepConfig config) : config_(std::move(config)) {}
            const startup::StepConfig& config() const override { return config_; }
            startup::StepResult run(startup::StepContext&) override {
                int count = ++call_count_;
                if (count == 1) {
                    return {startup::StepStatus::Failed, "First attempt failed"};
                }
                return {startup::StepStatus::Success, "Retry succeeded"};
            }
        };

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FailOnceThenSucceedStep>(
            startup::StepConfig{
                .id = "flaky_step",
                .display_name = "Flaky Step",
                .timeout = std::chrono::seconds{5},
                .critical = true
            }
        ));

        manager.set_steps(std::move(steps));

        // First run: fails
        manager.start_async();
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        auto progress1 = manager.get_progress();
        REQUIRE(progress1.state == startup::AppState::Off);
        REQUIRE(progress1.can_retry == true);

        // Retry: should succeed
        auto retry_result = manager.retry_async();
        REQUIRE(retry_result.ok);

        attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        auto progress2 = manager.get_progress();
        REQUIRE(progress2.state == startup::AppState::Active);
        REQUIRE(progress2.can_retry == false);
    }

    SECTION("retry_async fails when already running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Step that fails
        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}, .critical = true},
            startup::StepResult{startup::StepStatus::Failed, "Failed"}
        ));

        manager.set_steps(std::move(steps));

        // Run and fail
        manager.start_async();
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        // Start a slow retry
        std::vector<startup::StartupManager::StepPtr> slow_steps;
        slow_steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow", .display_name = "Slow", .timeout = std::chrono::seconds{5}}
        ));
        manager.clear_steps();
        manager.set_steps(std::move(slow_steps));

        auto result1 = manager.start_async();
        REQUIRE(result1.ok);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Try to retry while running
        auto result2 = manager.retry_async();
        REQUIRE_FALSE(result2.ok);
        REQUIRE(result2.error.find("already running") != std::string::npos);

        // Wait for completion
        attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
    }

}
TEST_CASE("Startup | StartupManager - Thread safety", "[startup][manager][async]") {

    SECTION("get_progress is safe while sequence runs") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        for (int i = 0; i < 5; ++i) {
            steps.push_back(std::make_unique<SlowRealStep>(
                startup::StepConfig{
                    .id = "step" + std::to_string(i),
                    .display_name = "Step " + std::to_string(i),
                    .timeout = std::chrono::seconds{5}
                }
            ));
        }

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Poll progress rapidly while sequence runs
        int poll_count = 0;
        while (manager.is_running() && poll_count < 50) {
            auto progress = manager.get_progress();  // Should not crash
            REQUIRE(progress.steps.size() == 5);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            poll_count++;
        }

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Final progress after completion
        auto final_progress = manager.get_progress();
        REQUIRE(final_progress.state == startup::AppState::Active);
    }

    SECTION("cannot modify steps while running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}}
        ));

        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit to ensure it's running
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Try to add step while running
        auto result = manager.add_step(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "step2", .display_name = "Step 2", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("running") != std::string::npos);

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
    }

}

TEST_CASE("Startup | StartupManager - Lifecycle", "[startup][manager]") {

    SECTION("destructor waits for running sequence") {
        EngineTestFixture f;

        {
            auto real_clock = std::make_shared<startup::SteadyClock>();
            startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

            std::vector<startup::StartupManager::StepPtr> steps;
            steps.push_back(std::make_unique<FakeStep>(
                startup::StepConfig{.id = "step1", .display_name = "Step 1", .timeout = std::chrono::seconds{5}},
                startup::StepResult{startup::StepStatus::Success, "OK"}
            ));

            manager.set_steps(std::move(steps));
            manager.start_async();

            // Destructor should wait for completion and clean up
        }

        // If we get here without hanging, destructor worked correctly
        REQUIRE(true);
    }

}

TEST_CASE("Startup | StartupManager - Edge cases", "[startup][manager]") {

    SECTION("get_state returns Off before any run") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        REQUIRE(manager.get_state() == startup::AppState::Off);
    }

    SECTION("can_retry returns false before any run") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        REQUIRE_FALSE(manager.can_retry());
    }

    SECTION("is_running returns false before any run") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        REQUIRE_FALSE(manager.is_running());
    }

    SECTION("get_progress returns empty progress before any run") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        auto progress = manager.get_progress();
        REQUIRE(progress.state == startup::AppState::Off);
        REQUIRE(progress.steps.empty());
    }

    SECTION("retry_async fails when can_retry is false") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        // Add step and run successfully
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
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // After success, can_retry should be false
        REQUIRE_FALSE(manager.can_retry());

        // Try to retry - should fail
        auto result = manager.retry_async();
        REQUIRE_FALSE(result.ok);
        REQUIRE_FALSE(result.error.empty());
    }

    SECTION("set_steps fails when sequence is running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow", .display_name = "Slow", .timeout = std::chrono::seconds{5}}
        ));
        manager.set_steps(std::move(steps));
        manager.start_async();

        // Wait a bit to ensure it's running
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Try to set_steps while running
        std::vector<startup::StartupManager::StepPtr> new_steps;
        new_steps.push_back(std::make_unique<FakeStep>(
            startup::StepConfig{.id = "new", .display_name = "New", .timeout = std::chrono::seconds{5}},
            startup::StepResult{startup::StepStatus::Success, "OK"}
        ));

        auto result = manager.set_steps(std::move(new_steps));
        REQUIRE_FALSE(result.ok);
        REQUIRE(result.error.find("running") != std::string::npos);

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
    }

    SECTION("clear_steps does nothing when sequence is running") {
        EngineTestFixture f;
        auto real_clock = std::make_shared<startup::SteadyClock>();
        startup::StartupManager manager(f.logger_startup, f.settings, real_clock);

        std::vector<startup::StartupManager::StepPtr> steps;
        steps.push_back(std::make_unique<SlowRealStep>(
            startup::StepConfig{.id = "slow", .display_name = "Slow", .timeout = std::chrono::seconds{5}}
        ));
        manager.set_steps(std::move(steps));

        // Get initial progress to verify steps are set
        auto initial_progress = manager.get_progress();
        REQUIRE(initial_progress.steps.size() == 1);

        manager.start_async();

        // Wait a bit to ensure it's running
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        REQUIRE(manager.is_running());

        // Try to clear_steps while running - should be ignored
        manager.clear_steps();

        // Wait for completion
        int attempts = 0;
        while (manager.is_running() && attempts < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Steps should still be there (clear was ignored)
        auto final_progress = manager.get_progress();
        REQUIRE(final_progress.steps.size() == 1);
        REQUIRE(final_progress.state == startup::AppState::Active);
    }

}