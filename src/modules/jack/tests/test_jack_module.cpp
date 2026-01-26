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
        auto result = module.init(f.settings, f.process_runner, f.client_api, f.device_manager);

        CHECK(result.ok);
        CHECK(module.get_state() == jack::JackModuleState::Initialized);
    }

    SECTION("Initialize fails if already initialized") {
        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);

        auto result = module.init(f.settings, f.process_runner, f.client_api, f.device_manager);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("already initialized"));
    }

    SECTION("Initialize validates settings") {
        f.settings.audio.num_channels = 0;  // Invalid

        auto result = module.init(f.settings, f.process_runner, f.client_api, f.device_manager);

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

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
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

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);

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

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        module.start();

        auto result = module.stop();

        CHECK(result.ok);
        CHECK(module.get_state() == jack::JackModuleState::Initialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("Stop is no-op when not active") {
        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);

        auto result = module.stop();

        CHECK(result.ok);
    }
}

TEST_CASE("Jack | JackModule - Audio level queries", "[jack][module]") {
    JackModuleTestFixture f;
    jack::JackModule module(f.logger);

    f.client_api->set_probe_result({48000, 256, true});
    module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
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
        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);

        CHECK(module.get_state() == jack::JackModuleState::Initialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("After start, state is Active and ready") {
        f.client_api->set_probe_result({48000, 256, true});
        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        module.start();

        CHECK(module.get_state() == jack::JackModuleState::Active);
        CHECK(module.is_ready());
    }
}

TEST_CASE("Jack | JackModule - Device validation", "[jack][module]") {

    SECTION("start succeeds with system_default devices") {
        JackModuleTestFixture f;
        jack::JackModule module(f.logger);

        f.client_api->set_probe_result({48000, 256, true});
        f.settings.audio.input_device_id = "system_default";
        f.settings.audio.output_device_id = "system_default";

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        auto result = module.start();

        CHECK(result.ok);
        CHECK(module.is_ready());
    }

    SECTION("start succeeds with valid specific devices") {
        JackModuleTestFixture f;
        jack::JackModule module(f.logger);

        // Add valid devices to the mock
        f.device_manager->add_device({
            .id = "Built-in Input",
            .name = "Built-in Input",
            .input_channels = 2,
            .output_channels = 0
        });
        f.device_manager->add_device({
            .id = "Built-in Output",
            .name = "Built-in Output",
            .input_channels = 0,
            .output_channels = 2
        });

        f.client_api->set_probe_result({48000, 256, true});
        f.settings.audio.input_device_id = "Built-in Input";
        f.settings.audio.output_device_id = "Built-in Output";

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        auto result = module.start();

        CHECK(result.ok);
        CHECK(module.is_ready());

        // Verify devices were queried
        CHECK(f.device_manager->get_device_call_count() >= 2);
    }

    SECTION("start falls back to system_default when input device missing") {
        JackModuleTestFixture f;
        jack::JackModule module(f.logger);

        f.client_api->set_probe_result({48000, 256, true});
        f.settings.audio.input_device_id = "Nonexistent Input";
        f.settings.audio.output_device_id = "system_default";

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        auto result = module.start();

        // Should succeed with fallback
        CHECK(result.ok);
        CHECK(module.is_ready());

        // Verify device manager was queried for the nonexistent device
        CHECK(f.device_manager->get_device_call_count() >= 1);
        CHECK(f.device_manager->get_last_device_id_queried() == "Nonexistent Input");
    }

    SECTION("start falls back to system_default when output device missing") {
        JackModuleTestFixture f;
        jack::JackModule module(f.logger);

        f.client_api->set_probe_result({48000, 256, true});
        f.settings.audio.input_device_id = "system_default";
        f.settings.audio.output_device_id = "Nonexistent Output";

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        auto result = module.start();

        // Should succeed with fallback
        CHECK(result.ok);
        CHECK(module.is_ready());

        // Verify device manager was queried for the nonexistent device
        CHECK(f.device_manager->get_device_call_count() >= 1);
    }

    SECTION("start falls back when device has no input capability") {
        JackModuleTestFixture f;
        jack::JackModule module(f.logger);

        // Add output-only device
        f.device_manager->add_device({
            .id = "Speakers Only",
            .name = "Speakers Only",
            .input_channels = 0,  // No input
            .output_channels = 8
        });

        f.client_api->set_probe_result({48000, 256, true});
        f.settings.audio.input_device_id = "Speakers Only";  // Wrong device type
        f.settings.audio.output_device_id = "system_default";

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        auto result = module.start();

        // Should succeed with fallback
        CHECK(result.ok);
        CHECK(module.is_ready());

        // Verify the device was queried
        CHECK(f.device_manager->get_device_call_count() >= 1);
    }

    SECTION("start falls back when device has no output capability") {
        JackModuleTestFixture f;
        jack::JackModule module(f.logger);

        // Add input-only device
        f.device_manager->add_device({
            .id = "Microphone Only",
            .name = "Microphone Only",
            .input_channels = 1,
            .output_channels = 0  // No output
        });

        f.client_api->set_probe_result({48000, 256, true});
        f.settings.audio.input_device_id = "system_default";
        f.settings.audio.output_device_id = "Microphone Only";  // Wrong device type

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        auto result = module.start();

        // Should succeed with fallback
        CHECK(result.ok);
        CHECK(module.is_ready());

        // Verify the device was queried
        CHECK(f.device_manager->get_device_call_count() >= 1);
    }

    SECTION("ServerConfig includes device IDs from settings") {
        JackModuleTestFixture f;
        jack::JackModule module(f.logger);

        // Add valid devices
        f.device_manager->add_device({
            .id = "USB Interface",
            .name = "USB Interface",
            .input_channels = 8,
            .output_channels = 8
        });

        f.client_api->set_probe_result({0, 0, false});  // No server
        f.client_api->set_probe_result_after_ready({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        f.settings.audio.input_device_id = "USB Interface";
        f.settings.audio.output_device_id = "USB Interface";

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        auto result = module.start();

        CHECK(result.ok);

        // Verify the spawn call included device flags
        auto spawn_calls = f.process_runner->get_spawn_calls();
        REQUIRE(spawn_calls.size() == 1);

        std::string args = spawn_calls[0].args_string();
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("-C"));
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("USB Interface"));
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("-P"));
    }

    SECTION("start with missing device queries device manager") {
        JackModuleTestFixture f;
        jack::JackModule module(f.logger);

        f.client_api->set_probe_result({48000, 256, true});
        f.settings.audio.input_device_id = "Missing Device";
        f.settings.audio.output_device_id = "system_default";

        // Reset call count
        auto initial_count = f.device_manager->get_device_call_count();

        module.init(f.settings, f.process_runner, f.client_api, f.device_manager);
        module.start();

        // Should have queried for the missing device
        CHECK(f.device_manager->get_device_call_count() > initial_count);
    }
}