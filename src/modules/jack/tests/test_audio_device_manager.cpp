//
// Created by Nicolas Désilles on 26/01/2026.
//

#include "helpers/test_fixtures.h"

using namespace aknet;
using namespace aknet::test;

// ============================================================================
// Interface Contract Tests (using Mock)
// ============================================================================

TEST_CASE("Jack | AudioDeviceManager - enumerate_devices returns system_default", "[jack][audio_device_manager]") {

    SECTION("default mock includes system_default device") {
        AudioDeviceManagerTestFixture f;

        auto devices = f.device_manager->enumerate_devices();

        REQUIRE_FALSE(devices.empty());
        CHECK(devices[0].id == "system_default");
        CHECK(devices[0].is_default);
        CHECK(f.device_manager->get_enumerate_call_count() == 1);
    }

    SECTION("system_default device has valid properties") {
        AudioDeviceManagerTestFixture f;

        auto devices = f.device_manager->enumerate_devices();
        auto system_default = devices[0];

        CHECK_FALSE(system_default.name.empty());
        CHECK(system_default.input_channels >= 0);
        CHECK(system_default.output_channels >= 0);
        CHECK(system_default.is_default);
    }
}

TEST_CASE("Jack | AudioDeviceManager - enumerate_devices returns hardware devices", "[jack][audio_device_manager]") {

    SECTION("mock can provide multiple devices") {
        AudioDeviceManagerTestFixture f;

        f.device_manager->add_device({
            .id = "Built-in Output",
            .name = "Built-in Output",
            .input_channels = 2,
            .output_channels = 2,
            .is_default = false
        });

        f.device_manager->add_device({
            .id = "USB Audio Device",
            .name = "Focusrite Scarlett 2i2",
            .input_channels = 2,
            .output_channels = 2,
            .is_default = false
        });

        auto devices = f.device_manager->enumerate_devices();

        REQUIRE(devices.size() == 3);  // system_default + 2 hardware
        CHECK(devices[1].id == "Built-in Output");
        CHECK(devices[2].id == "USB Audio Device");
    }

    SECTION("devices have correct channel counts") {
        AudioDeviceManagerTestFixture f;

        f.device_manager->add_device({
            .id = "input_only",
            .name = "Microphone",
            .input_channels = 1,
            .output_channels = 0,
            .is_default = false
        });

        f.device_manager->add_device({
            .id = "output_only",
            .name = "Speakers",
            .input_channels = 0,
            .output_channels = 8,
            .is_default = false
        });

        auto devices = f.device_manager->enumerate_devices();

        auto input_only = devices[1];
        CHECK(input_only.can_capture());
        CHECK_FALSE(input_only.can_playback());

        auto output_only = devices[2];
        CHECK_FALSE(output_only.can_capture());
        CHECK(output_only.can_playback());
    }
}

TEST_CASE("Jack | AudioDeviceManager - get_device_by_id lookup", "[jack][audio_device_manager]") {

    SECTION("returns device when ID exists") {
        AudioDeviceManagerTestFixture f;

        f.device_manager->add_device({
            .id = "test_device",
            .name = "Test Device",
            .input_channels = 2,
            .output_channels = 2,
            .is_default = false
        });

        auto device = f.device_manager->get_device_by_id("test_device");

        REQUIRE(device.has_value());
        CHECK(device->id == "test_device");
        CHECK(device->name == "Test Device");
        CHECK(f.device_manager->get_last_device_id_queried() == "test_device");
    }

    SECTION("returns nullopt when ID does not exist") {
        AudioDeviceManagerTestFixture f;

        auto device = f.device_manager->get_device_by_id("nonexistent");

        CHECK_FALSE(device.has_value());
    }

    SECTION("can find system_default by ID") {
        AudioDeviceManagerTestFixture f;

        auto device = f.device_manager->get_device_by_id("system_default");

        REQUIRE(device.has_value());
        CHECK(device->id == "system_default");
        CHECK(device->is_default);
    }
}

TEST_CASE("Jack | AudioDeviceManager - get_default_device", "[jack][audio_device_manager]") {

    SECTION("always returns a valid device") {
        AudioDeviceManagerTestFixture f;

        auto device = f.device_manager->get_default_device();

        CHECK(device.id == "system_default");
        CHECK(device.is_default);
        CHECK(f.device_manager->get_default_call_count() == 1);
    }
}

TEST_CASE("Jack | AudioDeviceManager - AudioDevice helper methods", "[jack][audio_device_manager]") {

    SECTION("can_capture returns true when input_channels > 0") {
        jack::AudioDevice device{
            .id = "mic",
            .name = "Microphone",
            .input_channels = 1,
            .output_channels = 0
        };

        CHECK(device.can_capture());
        CHECK_FALSE(device.can_playback());
    }

    SECTION("can_playback returns true when output_channels > 0") {
        jack::AudioDevice device{
            .id = "speakers",
            .name = "Speakers",
            .input_channels = 0,
            .output_channels = 2
        };

        CHECK_FALSE(device.can_capture());
        CHECK(device.can_playback());
    }

    SECTION("full-duplex device supports both") {
        jack::AudioDevice device{
            .id = "interface",
            .name = "Audio Interface",
            .input_channels = 8,
            .output_channels = 8
        };

        CHECK(device.can_capture());
        CHECK(device.can_playback());
    }
}

// ============================================================================
// CoreAudioDeviceManager Tests (Real Implementation)
// ============================================================================

#ifdef __APPLE__

TEST_CASE("Jack | CoreAudioDeviceManager - constructor validation", "[jack][audio_device_manager]") {

    SECTION("throws when logger is null") {
        REQUIRE_THROWS_AS(
            jack::create_audio_device_manager(nullptr),
            std::invalid_argument
        );
    }

    SECTION("succeeds with valid logger") {
        AudioDeviceManagerTestFixture f;

        REQUIRE_NOTHROW(
            jack::create_audio_device_manager(f.logger)
        );
    }
}

TEST_CASE("Jack | CoreAudioDeviceManager - enumerate real devices", "[jack][audio_device_manager]") {

    SECTION("enumeration includes at least system_default") {
        AudioDeviceManagerTestFixture f;
        auto manager = jack::create_audio_device_manager(f.logger);

        auto devices = manager->enumerate_devices();

        REQUIRE_FALSE(devices.empty());

        // First device should be system_default
        CHECK(devices[0].id == "system_default");
        CHECK(devices[0].is_default);
        CHECK_FALSE(devices[0].name.empty());
    }

    SECTION("enumeration finds built-in audio on macOS") {
        AudioDeviceManagerTestFixture f;
        auto manager = jack::create_audio_device_manager(f.logger);

        auto devices = manager->enumerate_devices();

        // macOS always has at least built-in audio + system_default
        CHECK(devices.size() >= 2);

        // At least one device should have output capability
        bool has_output_device = false;
        for (const auto& device : devices) {
            if (device.can_playback()) {
                has_output_device = true;
                break;
            }
        }
        CHECK(has_output_device);
    }

    SECTION("device IDs are non-empty and unique") {
        AudioDeviceManagerTestFixture f;
        auto manager = jack::create_audio_device_manager(f.logger);

        auto devices = manager->enumerate_devices();
        std::vector<std::string> ids;

        for (const auto& device : devices) {
            CHECK_FALSE(device.id.empty());
            CHECK_FALSE(device.name.empty());

            // Check uniqueness
            CHECK(std::find(ids.begin(), ids.end(), device.id) == ids.end());
            ids.push_back(device.id);
        }
    }

    SECTION("channel counts are non-negative") {
        AudioDeviceManagerTestFixture f;
        auto manager = jack::create_audio_device_manager(f.logger);

        auto devices = manager->enumerate_devices();

        for (const auto& device : devices) {
            CHECK(device.input_channels >= 0);
            CHECK(device.output_channels >= 0);
        }
    }
}

TEST_CASE("Jack | CoreAudioDeviceManager - get_device_by_id", "[jack][audio_device_manager]") {

    SECTION("can find system_default") {
        AudioDeviceManagerTestFixture f;
        auto manager = jack::create_audio_device_manager(f.logger);

        auto device = manager->get_device_by_id("system_default");

        REQUIRE(device.has_value());
        CHECK(device->id == "system_default");
        CHECK(device->is_default);
    }

    SECTION("returns nullopt for nonexistent device") {
        AudioDeviceManagerTestFixture f;
        auto manager = jack::create_audio_device_manager(f.logger);

        auto device = manager->get_device_by_id("definitely_does_not_exist_12345");

        CHECK_FALSE(device.has_value());
    }

    SECTION("can find device from enumeration by ID") {
        AudioDeviceManagerTestFixture f;
        auto manager = jack::create_audio_device_manager(f.logger);

        auto devices = manager->enumerate_devices();
        REQUIRE(devices.size() >= 2);  // At least system_default + one real device

        // Get a real device (not system_default)
        auto real_device = devices[1];

        // Look it up by ID
        auto found = manager->get_device_by_id(real_device.id);

        REQUIRE(found.has_value());
        CHECK(found->id == real_device.id);
        CHECK(found->name == real_device.name);
        CHECK(found->input_channels == real_device.input_channels);
        CHECK(found->output_channels == real_device.output_channels);
    }
}

TEST_CASE("Jack | CoreAudioDeviceManager - get_default_device", "[jack][audio_device_manager]") {

    SECTION("returns actual system default device from CoreAudio") {
        AudioDeviceManagerTestFixture f;
        auto manager = jack::create_audio_device_manager(f.logger);

        auto device = manager->get_default_device();

        // Should have a real device name (not just "System Default")
        CHECK_FALSE(device.name.empty());
        CHECK(device.name != "system_default");
        CHECK(device.is_default);

        // Should have real channel counts from the actual device
        CHECK(device.input_channels >= 0);
        CHECK(device.output_channels > 0);  // Default output should have outputs
    }
}

#endif // __APPLE__