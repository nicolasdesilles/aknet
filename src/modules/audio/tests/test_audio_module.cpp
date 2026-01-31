//
// Created by Nicolas Désilles on 31/01/2026.
//

#include "helpers/test_fixtures.h"
#include <audio_module.h>

using namespace aknet;
using namespace aknet::test;

TEST_CASE("Audio | AudioModule - Construction", "[audio][module]") {
    AudioModuleTestFixture f;

    SECTION("constructs with valid logger") {
        audio::AudioModule module(f.logger);

        CHECK(module.get_state() == audio::AudioModuleState::Uninitialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("constructor throws with null logger") {
        REQUIRE_THROWS_AS(
            audio::AudioModule(nullptr),
            std::invalid_argument
        );
    }
}

TEST_CASE("Audio | AudioModule - Initialization", "[audio][module]") {
    AudioModuleTestFixture f;
    audio::AudioModule module(f.logger);

    SECTION("initialize succeeds with valid settings") {
        auto result = module.init(f.settings);

        CHECK(result.ok);
        CHECK(module.get_state() == audio::AudioModuleState::Initialized);
    }

    SECTION("initialize fails if already initialized") {
        module.init(f.settings);

        auto result = module.init(f.settings);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("already initialized"));
    }

    SECTION("initialize validates num_channels") {
        f.settings.audio.num_channels = 0;  // Invalid

        auto result = module.init(f.settings);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("channel"));
    }

    SECTION("initialize validates buffer_size") {
        f.settings.audio.buffer_size = 0;  // Invalid

        auto result = module.init(f.settings);

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("buffer_size"));
    }
}

TEST_CASE("Audio | AudioModule - Start lifecycle", "[audio][module]") {
    AudioModuleTestFixture f;
    audio::AudioModule module(f.logger);

    SECTION("start fails if not initialized") {
        auto result = module.start();

        CHECK_FALSE(result.ok);
        CHECK_THAT(result.error, Catch::Matchers::ContainsSubstring("not initialized"));
    }

    SECTION("start succeeds after init") {
        module.init(f.settings);
        auto result = module.start();

        CHECK(result.ok);
        CHECK(module.get_state() == audio::AudioModuleState::Active);
        CHECK(module.is_ready());
    }

    SECTION("start is idempotent") {
        module.init(f.settings);
        module.start();

        auto result = module.start();  // Call again

        CHECK(result.ok);
        CHECK(module.is_ready());
    }
}

TEST_CASE("Audio | AudioModule - Stop lifecycle", "[audio][module]") {
    AudioModuleTestFixture f;
    audio::AudioModule module(f.logger);

    SECTION("stop when active") {
        module.init(f.settings);
        module.start();

        auto result = module.stop();

        CHECK(result.ok);
        CHECK(module.get_state() == audio::AudioModuleState::Initialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("stop is no-op when not active") {
        module.init(f.settings);

        auto result = module.stop();

        CHECK(result.ok);
    }
}

TEST_CASE("Audio | AudioModule - State queries", "[audio][module]") {
    AudioModuleTestFixture f;
    audio::AudioModule module(f.logger);

    SECTION("initial state is Uninitialized") {
        CHECK(module.get_state() == audio::AudioModuleState::Uninitialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("after init, state is Initialized") {
        module.init(f.settings);

        CHECK(module.get_state() == audio::AudioModuleState::Initialized);
        CHECK_FALSE(module.is_ready());
    }

    SECTION("after start, state is Active and ready") {
        module.init(f.settings);
        module.start();

        CHECK(module.get_state() == audio::AudioModuleState::Active);
        CHECK(module.is_ready());
    }
}

TEST_CASE("Audio | AudioModule - process_block stub", "[audio][module]") {
    AudioModuleTestFixture f;
    audio::AudioModule module(f.logger);

    SECTION("process_block doesn't crash when inactive") {
        // Create dummy buffers
        float ch0[256] = {0};
        float ch1[256] = {0};
        const float* buffers[] = {ch0, ch1};

        audio::AudioBlockView view{256, 2, buffers};

        // Should not crash
        module.process_block(view);
    }

    SECTION("process_block doesn't crash when active") {
        module.init(f.settings);
        module.start();

        // Create dummy buffers
        float ch0[256] = {0};
        float ch1[256] = {0};
        const float* buffers[] = {ch0, ch1};

        audio::AudioBlockView view{256, 2, buffers};

        // Should not crash (stub implementation)
        module.process_block(view);
    }
}