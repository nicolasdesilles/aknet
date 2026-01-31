//
// Created by Nicolas Désilles on 31/01/2026.
//

#include "helpers/test_fixtures.h"
#include <audio_buffer.h>
#include <audio_block_view.h>

using namespace aknet;
using namespace aknet::audio;

TEST_CASE("Audio | AudioBuffer - Construction", "[audio][buffer]") {

    SECTION("default construction creates empty buffer") {
        AudioBuffer buffer;

        CHECK(buffer.nframes() == 0);
        CHECK(buffer.num_channels() == 0);
        CHECK_FALSE(buffer.is_allocated());
    }

    SECTION("construction with size allocates storage") {
        AudioBuffer buffer(256, 2);

        CHECK(buffer.nframes() == 256);
        CHECK(buffer.num_channels() == 2);
        CHECK(buffer.is_allocated());
    }
}

TEST_CASE("Audio | AudioBuffer - Resize", "[audio][buffer]") {
    AudioBuffer buffer;

    SECTION("resize allocates storage") {
        buffer.resize(128, 4);

        CHECK(buffer.nframes() == 128);
        CHECK(buffer.num_channels() == 4);
        CHECK(buffer.is_allocated());
    }

    SECTION("resize to zero deallocates") {
        buffer.resize(256, 2);
        buffer.resize(0, 0);

        CHECK_FALSE(buffer.is_allocated());
    }
}

TEST_CASE("Audio | AudioBuffer - Channel access", "[audio][buffer]") {
    AudioBuffer buffer(256, 2);

    SECTION("valid channel access returns non-null") {
        CHECK(buffer.channel(0) != nullptr);
        CHECK(buffer.channel(1) != nullptr);
    }

    SECTION("invalid channel access returns null") {
        CHECK(buffer.channel(2) == nullptr);
        CHECK(buffer.channel(99) == nullptr);
    }

    SECTION("channel pointers are distinct") {
        float* ch0 = buffer.channel(0);
        float* ch1 = buffer.channel(1);

        CHECK(ch0 != ch1);
    }
}

TEST_CASE("Audio | AudioBuffer - Clear", "[audio][buffer]") {
    AudioBuffer buffer(256, 2);

    SECTION("clear sets all samples to zero") {
        // Write some non-zero data
        float* ch0 = buffer.channel(0);
        ch0[0] = 1.0f;
        ch0[100] = 0.5f;

        buffer.clear();

        // Verify all zeros
        for (uint32_t ch = 0; ch < buffer.num_channels(); ++ch) {
            const float* data = buffer.channel(ch);
            for (uint32_t i = 0; i < buffer.nframes(); ++i) {
                CHECK(data[i] == 0.0f);
            }
        }
    }
}

TEST_CASE("Audio | AudioBlockView - Validation", "[audio][buffer]") {

    SECTION("valid view") {
        float ch0[256] = {0};
        float ch1[256] = {0};
        const float* buffers[] = {ch0, ch1};

        AudioBlockView view{256, 2, buffers};

        CHECK(view.is_valid());
    }

    SECTION("invalid view - zero frames") {
        float ch0[256] = {0};
        const float* buffers[] = {ch0};

        AudioBlockView view{0, 1, buffers};

        CHECK_FALSE(view.is_valid());
    }

    SECTION("invalid view - null buffers") {
        AudioBlockView view{256, 2, nullptr};

        CHECK_FALSE(view.is_valid());
    }
}