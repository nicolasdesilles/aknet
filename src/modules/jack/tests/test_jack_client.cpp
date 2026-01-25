//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "test_fixtures.h"
#include <jack_client.h>

using namespace aknet;
using namespace aknet::test;

TEST_CASE("Jack | JackClient - Constructor validation", "[jack][client]") {

    SECTION("constructor throws when logger is null") {
        JackClientTestFixture f;

        REQUIRE_THROWS_AS(
            jack::JackClient(nullptr, f.client_api),
            std::invalid_argument
        );
    }

    SECTION("constructor throws when client_api is null") {
        JackClientTestFixture f;

        REQUIRE_THROWS_AS(
            jack::JackClient(f.logger, nullptr),
            std::invalid_argument
        );
    }

    SECTION("constructor succeeds with valid dependencies") {
        JackClientTestFixture f;

        REQUIRE_NOTHROW(
            jack::JackClient(f.logger, f.client_api)
        );
    }

    SECTION("initial state is Closed") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        CHECK(client.get_state() == jack::ClientState::Closed);
        CHECK_FALSE(client.is_active());
        CHECK(client.get_client_name().empty());
        CHECK(client.get_input_port_count() == 0);
    }

}

TEST_CASE("Jack | JackClient - Opening client", "[jack][client]") {

    SECTION("open succeeds with valid name") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        auto result = client.open("test_client");

        CHECK(result.ok);
        CHECK(client.get_state() == jack::ClientState::Open);
        CHECK(client.get_client_name() == "test_client");
        CHECK_FALSE(client.is_active());
    }

    SECTION("open fails when already open") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        auto result = client.open("another_client");

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("already open"));
        // Should keep original name
        CHECK(client.get_client_name() == "test_client");
    }

    SECTION("open fails with empty name") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        auto result = client.open("");

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("empty"));
        CHECK(client.get_state() == jack::ClientState::Closed);
    }

    SECTION("open delegates to client_api") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("my_client");

        CHECK(f.client_api->get_client_name() == "my_client");
    }

}

TEST_CASE("Jack | JackClient - Registering input ports", "[jack][client]") {

    SECTION("register_input_ports succeeds when client is open") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        auto result = client.register_input_ports(8);

        CHECK(result.ok);
        CHECK(client.get_input_port_count() == 8);
        CHECK(f.client_api->get_input_port_count() == 8);
    }

    SECTION("register_input_ports fails when client not open") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        auto result = client.register_input_ports(4);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("not open"));
        CHECK(client.get_input_port_count() == 0);
    }

    SECTION("register_input_ports fails when client is active") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        client.register_input_ports(4);
        client.activate();

        auto result = client.register_input_ports(8);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("active"));
        // Should keep original count
        CHECK(client.get_input_port_count() == 4);
    }

    SECTION("register_input_ports fails with invalid count") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        auto result = client.register_input_ports(0);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("invalid", Catch::CaseSensitive::No));
        CHECK(client.get_input_port_count() == 0);
    }

    SECTION("register_input_ports can be called multiple times before activation") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        client.register_input_ports(4);
        auto result = client.register_input_ports(8);

        CHECK(result.ok);
        CHECK(client.get_input_port_count() == 8);  // Should use latest
    }

}

TEST_CASE("Jack | JackClient - Activating client", "[jack][client]") {

    SECTION("activate succeeds when client is open") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        client.register_input_ports(2);
        auto result = client.activate();

        CHECK(result.ok);
        CHECK(client.get_state() == jack::ClientState::Active);
        CHECK(client.is_active());
    }

    SECTION("activate fails when client not open") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        auto result = client.activate();

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("not open"));
        CHECK_FALSE(client.is_active());
    }

    SECTION("activate fails when already active") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        client.activate();
        auto result = client.activate();

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("already active"));
    }

    SECTION("activate works without registering ports") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        // Don't register ports
        auto result = client.activate();

        CHECK(result.ok);  // Should succeed with 0 ports
        CHECK(client.is_active());
        CHECK(client.get_input_port_count() == 0);
    }

}

TEST_CASE("Jack | JackClient - Closing client", "[jack][client]") {

    SECTION("close succeeds when client is active") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        client.activate();
        auto result = client.close();

        CHECK(result.ok);
        CHECK(client.get_state() == jack::ClientState::Closed);
        CHECK_FALSE(client.is_active());
        CHECK(client.get_client_name().empty());
        CHECK(client.get_input_port_count() == 0);
    }

    SECTION("close succeeds when client is open but not active") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        auto result = client.close();

        CHECK(result.ok);
        CHECK(client.get_state() == jack::ClientState::Closed);
    }

    SECTION("close is no-op when already closed") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        auto result = client.close();

        CHECK(result.ok);
        CHECK(client.get_state() == jack::ClientState::Closed);
    }

    SECTION("close can be called multiple times safely") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_client");
        client.close();
        auto result = client.close();

        CHECK(result.ok);
    }

    SECTION("destructor closes client") {
        JackClientTestFixture f;

        {
            jack::JackClient client(f.logger, f.client_api);
            client.open("test_client");
            client.activate();
            // Destructor should close
        }

        // After destruction, mock should show client was closed
        CHECK_FALSE(f.client_api->is_active());
    }

}

TEST_CASE("Jack | JackClient - Full lifecycle", "[jack][client]") {

    SECTION("complete lifecycle: open, ports, activate, close") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        // Open
        auto open_result = client.open("my_client");
        REQUIRE(open_result.ok);
        CHECK(client.get_state() == jack::ClientState::Open);

        // Register ports
        auto ports_result = client.register_input_ports(4);
        REQUIRE(ports_result.ok);
        CHECK(client.get_input_port_count() == 4);

        // Activate
        auto activate_result = client.activate();
        REQUIRE(activate_result.ok);
        CHECK(client.is_active());

        // Close
        auto close_result = client.close();
        REQUIRE(close_result.ok);
        CHECK(client.get_state() == jack::ClientState::Closed);
    }

    SECTION("can reopen after closing") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        // First lifecycle
        client.open("client1");
        client.activate();
        client.close();

        // Second lifecycle
        auto result = client.open("client2");

        CHECK(result.ok);
        CHECK(client.get_state() == jack::ClientState::Open);
        CHECK(client.get_client_name() == "client2");
    }

}

TEST_CASE("Jack | JackClient - State queries", "[jack][client]") {

    SECTION("get_state reflects current state") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        CHECK(client.get_state() == jack::ClientState::Closed);

        client.open("test");
        CHECK(client.get_state() == jack::ClientState::Open);

        client.activate();
        CHECK(client.get_state() == jack::ClientState::Active);

        client.close();
        CHECK(client.get_state() == jack::ClientState::Closed);
    }

    SECTION("is_active matches state") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        CHECK_FALSE(client.is_active());

        client.open("test");
        CHECK_FALSE(client.is_active());

        client.activate();
        CHECK(client.is_active());

        client.close();
        CHECK_FALSE(client.is_active());
    }

    SECTION("get_client_name returns empty when closed") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test_name");
        CHECK(client.get_client_name() == "test_name");

        client.close();
        CHECK(client.get_client_name().empty());
    }

    SECTION("get_input_port_count resets on close") {
        JackClientTestFixture f;
        jack::JackClient client(f.logger, f.client_api);

        client.open("test");
        client.register_input_ports(8);
        CHECK(client.get_input_port_count() == 8);

        client.close();
        CHECK(client.get_input_port_count() == 0);
    }

}

TEST_CASE("Jack | JackClient - Set audio processor", "[jack][client]") {
    JackClientTestFixture f;
    jack::JackClient client(f.logger, f.client_api);

    client.open("test_client");
    client.register_input_ports(2);

    auto processor = std::make_shared<jack::JackAudioProcessor>(2);

    SECTION("successfully sets processor before activation") {
        auto result = client.set_audio_processor(processor);

        CHECK(result.ok);
        // Verify callback was registered with API
        CHECK(f.client_api->process_callback_ != nullptr);
    }

    SECTION("cannot set processor after activation") {
        client.set_audio_processor(processor);
        client.activate();

        auto processor2 = std::make_shared<jack::JackAudioProcessor>(2);
        auto result = client.set_audio_processor(processor2);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("after activation"));
    }

    SECTION("cannot set null processor") {
        auto result = client.set_audio_processor(nullptr);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("null"));
    }

    SECTION("processor channel count should match port count") {
        // Mismatch: 2 ports but processor expects 4 channels
        auto wrong_processor = std::make_shared<jack::JackAudioProcessor>(4);

        // This test verifies conceptual mismatch - actual audio data
        // won't cause crashes but meters for channels 2-3 will show silence
        // (This is informational, not a failure condition)
        auto result = client.set_audio_processor(wrong_processor);
        CHECK(result.ok);  // API allows this, but it's not recommended
    }
}

TEST_CASE("Jack | JackClient - Audio processing", "[jack][client]") {
    JackClientTestFixture f;
    jack::JackClient client(f.logger, f.client_api);

    client.open("test_client");
    client.register_input_ports(2);

    auto processor = std::make_shared<jack::JackAudioProcessor>(2);
    client.set_audio_processor(processor);
    client.activate();

    SECTION("can get audio levels") {
        auto levels = client.get_audio_levels();

        CHECK(levels.size() == 2);
        // Initial levels should be -inf dB (silence)
        CHECK(std::isinf(levels[0].rms_db));
        CHECK(std::isinf(levels[1].rms_db));
    }

    SECTION("can reset peak levels") {
        // Should not crash
        client.reset_peak_levels();

        auto levels = client.get_audio_levels();
        CHECK(levels.size() == 2);
    }

    SECTION("process callback updates levels (simulated)") {
        // Note: This is a basic test with the mock
        // Real audio data testing would require integration tests

        // Simulate JACK calling the process callback
        f.client_api->simulate_process_cycle(256);

        // Callback was invoked (no crash is success for this test)
        // Actual level values would require mock buffers
        auto levels = client.get_audio_levels();
        CHECK(levels.size() == 2);
    }

    SECTION("get_audio_levels returns empty if no processor set") {
        jack::JackClient client2(f.logger, f.client_api);

        auto levels = client2.get_audio_levels();
        CHECK(levels.empty());
    }
}

TEST_CASE("Jack | JackClient - Audio processor lifecycle", "[jack][client]") {
    JackClientTestFixture f;
    jack::JackClient client(f.logger, f.client_api);

    SECTION("processor survives close/reopen cycle") {
        client.open("test_client");
        client.register_input_ports(2);

        auto processor = std::make_shared<jack::JackAudioProcessor>(2);
        client.set_audio_processor(processor);
        client.activate();

        // Process some audio (simulated)
        f.client_api->simulate_process_cycle(256);

        // Close client
        client.close();

        // Reopen and set processor again
        client.open("test_client2");
        client.register_input_ports(2);

        auto result = client.set_audio_processor(processor);
        CHECK(result.ok);
    }
}