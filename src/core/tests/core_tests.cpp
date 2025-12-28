//
// Created by Nicolas Désilles on 28/12/2025.
//

#include <catch2/catch_test_macros.hpp>
#include <core.h>

using namespace aknet;

TEST_CASE("Core | Lifecycle", "[core]") {

    SECTION("init and shutdown complete without error") {
        core c;

        // Should not throw
        REQUIRE_NOTHROW(c.init());
        REQUIRE_NOTHROW(c.shutdown());
    }

    SECTION("double shutdown is safe") {
        core c;
        c.init();
        c.shutdown();

        // Second shutdown should be safe
        REQUIRE_NOTHROW(c.shutdown());
    }
}