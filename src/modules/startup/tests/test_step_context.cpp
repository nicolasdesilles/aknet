//
// Created by Nicolas Désilles on 23/01/2026.
//

#include "test_fixtures.h"

using namespace aknet;
using namespace aknet::test;

// ------------------------------------------------------------------------------------------------
// StepContext Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Startup | StepContext edge cases", "[startup][context]") {

    SECTION("abort_requested returns false when abort_flag is null") {
        startup::StepContext ctx;
        ctx.abort_flag = nullptr;
        
        REQUIRE_FALSE(ctx.abort_requested());
    }

    SECTION("abort_requested returns true when flag is set") {
        std::atomic_bool abort_flag{true};
        startup::StepContext ctx;
        ctx.abort_flag = &abort_flag;
        
        REQUIRE(ctx.abort_requested());
    }

    SECTION("abort_requested returns false when flag is not set") {
        std::atomic_bool abort_flag{false};
        startup::StepContext ctx;
        ctx.abort_flag = &abort_flag;
        
        REQUIRE_FALSE(ctx.abort_requested());
    }

    SECTION("is_expired returns false when clock is null") {
        startup::StepContext ctx;
        ctx.clock = nullptr;
        
        REQUIRE_FALSE(ctx.is_expired());
    }

    SECTION("is_expired returns true when past deadline") {
        auto clock = std::make_shared<FakeClock>();
        clock->t = std::chrono::steady_clock::now();
        
        startup::StepContext ctx;
        ctx.clock = clock.get();
        ctx.deadline = clock->t - std::chrono::seconds{1};  // Deadline in the past
        
        REQUIRE(ctx.is_expired());
    }

    SECTION("is_expired returns false when before deadline") {
        auto clock = std::make_shared<FakeClock>();
        clock->t = std::chrono::steady_clock::now();
        
        startup::StepContext ctx;
        ctx.clock = clock.get();
        ctx.deadline = clock->t + std::chrono::seconds{10};  // Deadline in the future
        
        REQUIRE_FALSE(ctx.is_expired());
    }

    SECTION("check_abort_point does nothing when abort_flag is null") {
        startup::StepContext ctx;
        ctx.abort_flag = nullptr;
        
        // Should not throw
        REQUIRE_NOTHROW(ctx.check_abort_point());
    }

    SECTION("check_abort_point does nothing when abort not requested") {
        std::atomic_bool abort_flag{false};
        startup::StepContext ctx;
        ctx.abort_flag = &abort_flag;
        
        // Should not throw
        REQUIRE_NOTHROW(ctx.check_abort_point());
    }

    SECTION("check_abort_point throws when abort requested") {
        std::atomic_bool abort_flag{true};
        startup::StepContext ctx;
        ctx.abort_flag = &abort_flag;
        
        // Should throw
        REQUIRE_THROWS_AS(ctx.check_abort_point(), std::runtime_error);
    }

}
