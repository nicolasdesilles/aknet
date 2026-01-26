//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "helpers/test_fixtures.h"

using namespace aknet;
using namespace aknet::test;

// ------------------------------------------------------------------------------------------------
// JackServerManager Constructor Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Jack | ServerManager - Constructor validation", "[jack][server_manager]") {

    SECTION("constructor throws when logger is null") {
        JackServerManagerTestFixture f;

        REQUIRE_THROWS_AS(
            jack::JackServerManager(nullptr, f.client_api, f.process_runner),
            std::invalid_argument
        );
    }

    SECTION("constructor throws when client_api is null") {
        JackServerManagerTestFixture f;

        REQUIRE_THROWS_AS(
            jack::JackServerManager(f.logger, nullptr, f.process_runner),
            std::invalid_argument
        );
    }

    SECTION("constructor throws when process_runner is null") {
        JackServerManagerTestFixture f;

        REQUIRE_THROWS_AS(
            jack::JackServerManager(f.logger, f.client_api, nullptr),
            std::invalid_argument
        );
    }

    SECTION("constructor succeeds with valid dependencies") {
        JackServerManagerTestFixture f;

        REQUIRE_NOTHROW(
            jack::JackServerManager(f.logger, f.client_api, f.process_runner)
        );
    }

}

TEST_CASE("Jack | ServerManager - Server probing", "[jack][server_manager]") {

    SECTION("probe_server returns info when server is running") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({48000, 256, true});

        auto info = manager.probe_server();

        CHECK(info.is_running);
        CHECK(info.sample_rate == 48000);
        CHECK(info.buffer_size == 256);
        CHECK(f.client_api->get_probe_call_count() == 1);
    }

    SECTION("probe_server returns not running when server is down") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({0, 0, false});

        auto info = manager.probe_server();

        CHECK_FALSE(info.is_running);
        CHECK(f.client_api->get_probe_call_count() == 1);
    }

    SECTION("probe_server delegates to client_api") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({96000, 1024, true});

        auto info = manager.probe_server();

        CHECK(info.sample_rate == 96000);
        CHECK(info.buffer_size == 1024);
    }

}

TEST_CASE("Jack | ServerManager - Starting server when not running", "[jack][server_manager]") {

    SECTION("ensure_server starts server with correct arguments") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({0, 0, false});  // Server not running
        f.client_api->set_probe_result_after_ready({48000, 512, true});  // Server ready after spawn
        f.process_runner->set_spawn_result(true, 1234);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        jack::ServerConfig config{
            .executable_path = "/opt/homebrew/bin/jackd",
            .sample_rate = 48000,
            .buffer_size = 512
        };

        auto result = manager.ensure_server(config);

        CHECK(result.ok);
        CHECK(manager.owns_server());

        // Verify spawn was called once
        auto spawn_calls = f.process_runner->get_spawn_calls();
        REQUIRE(spawn_calls.size() == 1);
        CHECK(spawn_calls[0].executable == "/opt/homebrew/bin/jackd");

        // Verify args contain required flags for macOS CoreAudio
        std::string args = spawn_calls[0].args_string();
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("-R"));  // Realtime
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("-d coreaudio"));  // Driver
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("-r 48000"));  // Sample rate
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("-p 512"));  // Buffer size
    }

    SECTION("ensure_server fails when spawn fails") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({0, 0, false});
        f.process_runner->set_spawn_result(false, 0);

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};

        auto result = manager.ensure_server(config);

        CHECK_FALSE(result.ok);
        CHECK_FALSE(manager.owns_server());
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("failed"));
    }

}

TEST_CASE("Jack | ServerManager - Server already running with correct settings", "[jack][server_manager]") {

    SECTION("ensure_server does nothing when server matches config") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({48000, 256, true});  // Already running with target

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};

        auto result = manager.ensure_server(config);

        CHECK(result.ok);
        CHECK_FALSE(manager.owns_server());  // We didn't start it
        CHECK(f.process_runner->get_spawn_calls().empty());  // No spawn
    }

}

TEST_CASE("Jack | ServerManager - Server running with wrong settings", "[jack][server_manager]") {

    SECTION("ensure_server fails when external server has wrong sample rate") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({44100, 256, true});  // Wrong sample rate

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};

        auto result = manager.ensure_server(config, false);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("mismatch"));
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("44100"));
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("48000"));
        CHECK_FALSE(manager.owns_server());
    }

    SECTION("ensure_server fails when external server has wrong buffer size") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({48000, 128, true});  // Wrong buffer

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};

        auto result = manager.ensure_server(config, false);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("mismatch"));
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("128"));
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("256"));
    }

    SECTION("ensure_server fails with force=true for external server") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({44100, 128, true});  // Wrong settings

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};

        auto result = manager.ensure_server(config, true);

        // Even with force=true, we cannot kill external servers
        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("external"));
    }

}

TEST_CASE("Jack | ServerManager - Restarting owned server", "[jack][server_manager]") {

    SECTION("ensure_server restarts owned server with wrong settings") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        // Start server initially with one config
        f.client_api->set_probe_result({0, 0, false});
        f.client_api->set_probe_result_after_ready({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        jack::ServerConfig config1{"/opt/homebrew/bin/jackd", 48000, 256};
        auto result1 = manager.ensure_server(config1);
        REQUIRE(result1.ok);
        REQUIRE(manager.owns_server());

        // Now simulate server running with our old settings (already set by mark_server_ready)

        // Try to ensure server with new settings - update the ready state for restart
        jack::ServerConfig config2{"/opt/homebrew/bin/jackd", 96000, 512};
        f.client_api->set_probe_result_after_ready({96000, 512, true});
        f.process_runner->set_spawn_result(true, 5678);

        auto result2 = manager.ensure_server(config2);

        CHECK(result2.ok);
        CHECK(manager.owns_server());

        // Verify terminate was called on old PID
        auto terminate_calls = f.process_runner->get_terminate_calls();
        REQUIRE(terminate_calls.size() == 1);
        CHECK(terminate_calls[0].pid == 1234);

        // Verify spawn was called again with new settings
        auto spawn_calls = f.process_runner->get_spawn_calls();
        REQUIRE(spawn_calls.size() == 2);  // Initial + restart
        std::string args = spawn_calls[1].args_string();
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("-r 96000"));
        CHECK_THAT(args, Catch::Matchers::ContainsSubstring("-p 512"));
    }

}

TEST_CASE("Jack | ServerManager - Stopping server", "[jack][server_manager]") {

    SECTION("stop_server terminates owned process") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        // Start a server
        f.client_api->set_probe_result({0, 0, false});
        f.client_api->set_probe_result_after_ready({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};
        manager.ensure_server(config);
        REQUIRE(manager.owns_server());

        // Stop it
        auto result = manager.stop_server();

        CHECK(result.ok);
        CHECK_FALSE(manager.owns_server());

        // Verify terminate was called
        auto terminate_calls = f.process_runner->get_terminate_calls();
        REQUIRE(terminate_calls.size() == 1);
        CHECK(terminate_calls[0].pid == 1234);
        CHECK_FALSE(terminate_calls[0].force);  // Should use SIGTERM first
    }

    SECTION("stop_server is no-op when no server owned") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        auto result = manager.stop_server();

        CHECK(result.ok);
        CHECK(f.process_runner->get_terminate_calls().empty());
    }

    SECTION("destructor stops owned server") {
        JackServerManagerTestFixture f;

        {
            jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

            f.client_api->set_probe_result({0, 0, false});
            f.client_api->set_probe_result_after_ready({48000, 256, true});
            f.process_runner->set_spawn_result(true, 1234);
            f.process_runner->set_on_spawn_callback([&]() {
                f.client_api->mark_server_ready();
            });

            jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};
            manager.ensure_server(config);
            REQUIRE(manager.owns_server());

            // Destructor should terminate
        }

        // Verify terminate was called
        auto terminate_calls = f.process_runner->get_terminate_calls();
        REQUIRE(terminate_calls.size() == 1);
        CHECK(terminate_calls[0].pid == 1234);
    }

}

TEST_CASE("Jack | ServerManager - Ownership tracking", "[jack][server_manager]") {

    SECTION("owns_server is false initially") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        CHECK_FALSE(manager.owns_server());
    }

    SECTION("owns_server is true after starting server") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({0, 0, false});
        f.client_api->set_probe_result_after_ready({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};
        manager.ensure_server(config);

        CHECK(manager.owns_server());
    }

    SECTION("owns_server is false when server was already running") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({48000, 256, true});  // Already running

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};
        manager.ensure_server(config);

        CHECK_FALSE(manager.owns_server());
    }

    SECTION("owns_server becomes false after stop_server") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({0, 0, false});
        f.client_api->set_probe_result_after_ready({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};
        manager.ensure_server(config);
        REQUIRE(manager.owns_server());

        manager.stop_server();

        CHECK_FALSE(manager.owns_server());
    }

}

TEST_CASE("Jack | ServerManager - Multiple ensure_server calls", "[jack][server_manager]") {

    SECTION("calling ensure_server twice with same config is idempotent") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        f.client_api->set_probe_result({0, 0, false});
        f.client_api->set_probe_result_after_ready({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        jack::ServerConfig config{"/opt/homebrew/bin/jackd", 48000, 256};

        auto result1 = manager.ensure_server(config);
        CHECK(result1.ok);

        // Server is now running with our settings (set by mark_server_ready)

        auto result2 = manager.ensure_server(config);
        CHECK(result2.ok);

        // Should only have spawned once
        CHECK(f.process_runner->get_spawn_calls().size() == 1);
    }

    SECTION("calling ensure_server with different config triggers restart") {
        JackServerManagerTestFixture f;
        jack::JackServerManager manager(f.logger, f.client_api, f.process_runner);

        // Start with first config
        f.client_api->set_probe_result({0, 0, false});
        f.client_api->set_probe_result_after_ready({48000, 256, true});
        f.process_runner->set_spawn_result(true, 1234);
        f.process_runner->set_on_spawn_callback([&]() {
            f.client_api->mark_server_ready();
        });

        jack::ServerConfig config1{"/opt/homebrew/bin/jackd", 48000, 256};
        manager.ensure_server(config1);

        // Now server is running with first config (set by mark_server_ready)
        // Update ready state for restart
        f.client_api->set_probe_result_after_ready({96000, 512, true});
        f.process_runner->set_spawn_result(true, 5678);

        // Try different config
        jack::ServerConfig config2{"/opt/homebrew/bin/jackd", 96000, 512};
        auto result = manager.ensure_server(config2);

        CHECK(result.ok);
        CHECK(f.process_runner->get_spawn_calls().size() == 2);
        CHECK(f.process_runner->get_terminate_calls().size() == 1);
    }

}

