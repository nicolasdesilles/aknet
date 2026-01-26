//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "helpers/test_fixtures.h"
#include <jack_module.h>

using namespace aknet;
using namespace aknet::test;



TEST_CASE("Jack | JackModule - Construction", "[jack][module]") {
    JackModuleTestFixture f;

    SECTION("Constructs with valid logger") {
        jack::JackModule module(f.logger);

        CHECK(module.get_state() == jack::JackModuleState::Uninitialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("Constructor throws with null logger") {
        REQUIRE_THROWS_AS(
            jack::JackModule(nullptr),
            std::invalid_argument
        );
    }
}

TEST_CASE("Jack | JackModule - Initialization", "[jack][module]") {
    JackModuleTestFixture f;
    jack::JackModule module(f.logger);

    SECTION("Initialize succeeds with valid settings") {
        auto result = module.init(f.settings, f.process_runner, f.client_api);

        CHECK(result.ok);
        CHECK(module.get_state() == jack::JackModuleState::Initialized);
    }

    SECTION("Initialize fails if already initialized") {
        module.init(f.settings, f.process_runner, f.client_api);

        auto result = module.init(f.settings, f.process_runner, f.client_api);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("already initialized"));
    }

    SECTION("Initialize validates settings") {
        f.settings.audio.num_channels = 0;  // Invalid

        auto result = module.init(f.settings, f.process_runner, f.client_api);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("channel"));
    }
}

TEST_CASE("Jack | JackModule - Start lifecycle", "[jack][module]") {
    JackModuleTestFixture f;
    jack::JackModule module(f.logger);

    SECTION("Start fails if not initialized") {
        auto result = module.start();

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("not initialized"));
    }

    SECTION("Start succeeds with server running") {
        // Mock: server already running with correct settings
        f.client_api->set_probe_result({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);

        module.init(f.settings, f.process_runner, f.client_api);
        auto result = module.start();

        CHECK(result.ok);
        CHECK(module.get_state() == jack::JackModuleState::Active);
        CHECK(module.is_ready());
    }

    SECTION("Start launches server if not running") {
        // Mock: no server running, but becomes ready after spawn
        f.client_api->set_probe_result({0, 0, false});
        f.client_api->set_probe_result_after_ready({48000, 256, true});
        f.process_runner->set_spawn_result(true, 5678);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        module.init(f.settings, f.process_runner, f.client_api);

        auto result = module.start();

        // Verify spawn was called and start succeeded
        CHECK(f.process_runner->get_spawn_calls().size() >= 1);
        CHECK(result.ok);
        CHECK(module.is_ready());
    }
}

TEST_CASE("Jack | JackModule - Stop lifecycle", "[jack][module]") {
    JackModuleTestFixture f;
    jack::JackModule module(f.logger);

    SECTION("Stop when active") {
        f.client_api->set_probe_result({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);

        module.init(f.settings, f.process_runner, f.client_api);
        module.start();

        auto result = module.stop();

        CHECK(result.ok);
        CHECK(module.get_state() == jack::JackModuleState::Initialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("Stop is no-op when not active") {
        module.init(f.settings, f.process_runner, f.client_api);

        auto result = module.stop();

        CHECK(result.ok);
    }
}

TEST_CASE("Jack | JackModule - Audio level queries", "[jack][module]") {
    JackModuleTestFixture f;
    jack::JackModule module(f.logger);

    f.client_api->set_probe_result({48000, 256, true});
    module.init(f.settings, f.process_runner, f.client_api);
    module.start();

    SECTION("get_audio_levels returns correct size") {
        auto levels = module.get_audio_levels();

        CHECK(levels.size() == 2);  // num_channels from settings
    }

    SECTION("get_peak_levels returns correct size") {
        auto peaks = module.get_peak_levels();

        CHECK(peaks.size() == 2);
    }

    SECTION("reset_peaks does not crash") {
        module.reset_peaks();
        // Should not throw
    }
}

TEST_CASE("Jack | JackModule - State queries", "[jack][module]") {
    JackModuleTestFixture f;
    jack::JackModule module(f.logger);

    SECTION("Initial state is Uninitialized") {
        CHECK(module.get_state() == jack::JackModuleState::Uninitialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("After init, state is Initialized") {
        module.init(f.settings, f.process_runner, f.client_api);

        CHECK(module.get_state() == jack::JackModuleState::Initialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("After start, state is Active and ready") {
        f.client_api->set_probe_result({48000, 256, true});
        module.init(f.settings, f.process_runner, f.client_api);
        module.start();

        CHECK(module.get_state() == jack::JackModuleState::Active);
        CHECK(module.is_ready());
    }
}