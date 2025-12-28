//
// Created by Nicolas Désilles on 28/12/2025.
//

#include <catch2/catch_test_macros.hpp>
#include <core.h>
#include <logger.h>

using namespace aknet;

TEST_CASE("Integration | Full application lifecycle", "[integration]") {

    SECTION("complete init-use-shutdown cycle") {
        core app;

        // Initialize
        REQUIRE_NOTHROW(app.init());

        // Use features
        REQUIRE_NOTHROW(core::test_function());

        // Shutdown
        REQUIRE_NOTHROW(app.shutdown());
    }
}