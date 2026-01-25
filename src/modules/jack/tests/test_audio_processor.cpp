//
// Created by Nicolas Désilles on 25/01/2026.
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <jack_audio_processor.h>

#include <cmath>
#include <vector>
#include <numbers>

using namespace aknet::jack;
using Catch::Matchers::WithinAbs;

// ------------------------------------------------------------------------------------------------
// Test Helpers
// ------------------------------------------------------------------------------------------------

// Helper: Generate sine wave
std::vector<float> generate_sine_wave(int num_samples, float frequency, float amplitude, float sample_rate) {
    std::vector<float> buffer(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        buffer[i] = amplitude * std::sin(2.0f * std::numbers::pi_v<float> * frequency * t);
    }
    return buffer;
}

// Helper: Generate DC offset (constant value)
std::vector<float> generate_dc(int num_samples, float value) {
    std::vector<float> buffer(num_samples, value);
    return buffer;
}

// Helper: Generate silence
std::vector<float> generate_silence(int num_samples) {
    std::vector<float> buffer(num_samples, 0.0f);
    return buffer;
}

// ------------------------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------------------------

TEST_CASE("Jack | AudioProcessor - Construction", "[jack][audio_processor]") {
    SECTION("Constructs with valid channel count") {
        JackAudioProcessor processor(2);
        REQUIRE(processor.get_channel_count() == 2);
    }

    SECTION("Constructs with single channel") {
        JackAudioProcessor processor(1);
        REQUIRE(processor.get_channel_count() == 1);
    }

    SECTION("Constructs with many channels") {
        JackAudioProcessor processor(128);
        REQUIRE(processor.get_channel_count() == 128);
    }
}

TEST_CASE("Jack | AudioProcessor - Initial state", "[jack][audio_processor]") {
    JackAudioProcessor processor(2);
    auto meters = processor.get_meters();

    SECTION("Returns correct number of meters") {
        REQUIRE(meters.size() == 2);
    }

    SECTION("Initial RMS levels are -inf dB") {
        REQUIRE(std::isinf(meters[0].rms_db));
        REQUIRE(meters[0].rms_db < 0);
        REQUIRE(std::isinf(meters[1].rms_db));
        REQUIRE(meters[1].rms_db < 0);
    }

    SECTION("Initial peak levels are -inf dB") {
        REQUIRE(std::isinf(meters[0].peak_db));
        REQUIRE(meters[0].peak_db < 0);
        REQUIRE(std::isinf(meters[1].peak_db));
        REQUIRE(meters[1].peak_db < 0);
    }
}

TEST_CASE("Jack | AudioProcessor - Process silence", "[jack][audio_processor]") {
    JackAudioProcessor processor(2);

    auto silence_ch0 = generate_silence(256);
    auto silence_ch1 = generate_silence(256);
    const float* buffers[] = { silence_ch0.data(), silence_ch1.data() };

    processor.process(256, buffers);
    auto meters = processor.get_meters();

    SECTION("RMS of silence is -inf dB") {
        REQUIRE(std::isinf(meters[0].rms_db));
        REQUIRE(meters[0].rms_db < 0);
        REQUIRE(std::isinf(meters[1].rms_db));
        REQUIRE(meters[1].rms_db < 0);
    }

    SECTION("Peak of silence is -inf dB") {
        REQUIRE(std::isinf(meters[0].peak_db));
        REQUIRE(meters[0].peak_db < 0);
        REQUIRE(std::isinf(meters[1].peak_db));
        REQUIRE(meters[1].peak_db < 0);
    }
}

TEST_CASE("Jack | AudioProcessor - Process DC signal", "[jack][audio_processor]") {
    JackAudioProcessor processor(1);

    SECTION("DC at 0.5 amplitude") {
        auto dc = generate_dc(256, 0.5f);
        const float* buffers[] = { dc.data() };

        processor.process(256, buffers);
        auto meters = processor.get_meters();

        // RMS of DC = DC value = 0.5
        // 0.5 in dB = 20 * log10(0.5) ≈ -6.02 dB
        REQUIRE_THAT(meters[0].rms_db, WithinAbs(-6.02f, 0.1f));

        // Peak of DC = |0.5| = 0.5
        REQUIRE_THAT(meters[0].peak_db, WithinAbs(-6.02f, 0.1f));
    }

    SECTION("DC at 1.0 amplitude (0 dBFS)") {
        auto dc = generate_dc(256, 1.0f);
        const float* buffers[] = { dc.data() };

        processor.process(256, buffers);
        auto meters = processor.get_meters();

        // RMS of DC = 1.0 = 0 dB
        REQUIRE_THAT(meters[0].rms_db, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(meters[0].peak_db, WithinAbs(0.0f, 0.01f));
    }

    SECTION("DC at 0.1 amplitude") {
        auto dc = generate_dc(256, 0.1f);
        const float* buffers[] = { dc.data() };

        processor.process(256, buffers);
        auto meters = processor.get_meters();

        // 0.1 in dB = 20 * log10(0.1) = -20 dB
        REQUIRE_THAT(meters[0].rms_db, WithinAbs(-20.0f, 0.1f));
        REQUIRE_THAT(meters[0].peak_db, WithinAbs(-20.0f, 0.1f));
    }
}

TEST_CASE("Jack | AudioProcessor - Process sine wave", "[jack][audio_processor]") {
    JackAudioProcessor processor(1);

    // Generate 1kHz sine at 48kHz sample rate, 0.707 amplitude
    // RMS of sine = amplitude / sqrt(2) = 0.707 / 1.414 ≈ 0.5
    // 0.5 in dB ≈ -6 dB
    auto sine = generate_sine_wave(256, 1000.0f, 0.707f, 48000.0f);
    const float* buffers[] = { sine.data() };

    processor.process(256, buffers);
    auto meters = processor.get_meters();

    SECTION("RMS of 0.707 amplitude sine is ~-6 dB") {
        REQUIRE_THAT(meters[0].rms_db, WithinAbs(-6.0f, 0.5f));
    }

    SECTION("Peak of 0.707 amplitude sine is ~-3 dB") {
        // Peak = 0.707 = 20*log10(0.707) ≈ -3.01 dB
        REQUIRE_THAT(meters[0].peak_db, WithinAbs(-3.0f, 0.5f));
    }
}

TEST_CASE("Jack | AudioProcessor - Multi-channel processing", "[jack][audio_processor]") {
    JackAudioProcessor processor(3);

    // Channel 0: Silence
    auto silence = generate_silence(256);
    // Channel 1: DC at 0.5
    auto dc = generate_dc(256, 0.5f);
    // Channel 2: DC at 1.0
    auto full_scale = generate_dc(256, 1.0f);

    const float* buffers[] = { silence.data(), dc.data(), full_scale.data() };

    processor.process(256, buffers);
    auto meters = processor.get_meters();

    REQUIRE(meters.size() == 3);

    SECTION("Channel 0 (silence) is -inf dB") {
        REQUIRE(std::isinf(meters[0].rms_db));
        REQUIRE(meters[0].rms_db < 0);
    }

    SECTION("Channel 1 (0.5) is ~-6 dB") {
        REQUIRE_THAT(meters[1].rms_db, WithinAbs(-6.0f, 0.1f));
    }

    SECTION("Channel 2 (1.0) is 0 dB") {
        REQUIRE_THAT(meters[2].rms_db, WithinAbs(0.0f, 0.1f));
    }
}

TEST_CASE("Jack | AudioProcessor - Peak hold and decay", "[jack][audio_processor]") {
    JackAudioProcessor processor(1);

    SECTION("Peak holds across multiple process calls") {
        // First call: loud signal
        auto loud = generate_dc(256, 1.0f);
        const float* buffers_loud[] = { loud.data() };
        processor.process(256, buffers_loud);

        auto meters1 = processor.get_meters();
        REQUIRE_THAT(meters1[0].peak_db, WithinAbs(0.0f, 0.1f));

        // Second call: quiet signal (peak should decay slightly but stay high)
        auto quiet = generate_dc(256, 0.1f);
        const float* buffers_quiet[] = { quiet.data() };
        processor.process(256, buffers_quiet);

        auto meters2 = processor.get_meters();
        // Peak should still be close to 0 dB (decay is very slow)
        REQUIRE(meters2[0].peak_db > -1.0f);  // Still near 0 dB
        REQUIRE(meters2[0].peak_db < 0.1f);   // But decaying
    }

    SECTION("Peak increases immediately with louder signal") {
        // Start with quiet
        auto quiet = generate_dc(256, 0.1f);
        const float* buffers_quiet[] = { quiet.data() };
        processor.process(256, buffers_quiet);

        auto meters1 = processor.get_meters();
        float initial_peak = meters1[0].peak_db;

        // Then loud
        auto loud = generate_dc(256, 1.0f);
        const float* buffers_loud[] = { loud.data() };
        processor.process(256, buffers_loud);

        auto meters2 = processor.get_meters();
        REQUIRE(meters2[0].peak_db > initial_peak);
        REQUIRE_THAT(meters2[0].peak_db, WithinAbs(0.0f, 0.1f));
    }
}

TEST_CASE("Jack | AudioProcessor - Reset peaks", "[jack][audio_processor]") {
    JackAudioProcessor processor(2);

    // Set some peak values
    auto loud_ch0 = generate_dc(256, 1.0f);
    auto loud_ch1 = generate_dc(256, 0.5f);
    const float* buffers[] = { loud_ch0.data(), loud_ch1.data() };
    processor.process(256, buffers);

    auto meters_before = processor.get_meters();
    REQUIRE_THAT(meters_before[0].peak_db, WithinAbs(0.0f, 0.1f));
    REQUIRE_THAT(meters_before[1].peak_db, WithinAbs(-6.0f, 0.1f));

    // Reset
    processor.reset_peaks();

    // Process silence
    auto silence_ch0 = generate_silence(256);
    auto silence_ch1 = generate_silence(256);
    const float* buffers_silence[] = { silence_ch0.data(), silence_ch1.data() };
    processor.process(256, buffers_silence);

    auto meters_after = processor.get_meters();
    REQUIRE(std::isinf(meters_after[0].peak_db));
    REQUIRE(std::isinf(meters_after[1].peak_db));
}

TEST_CASE("Jack | AudioProcessor - Clipping detection", "[jack][audio_processor]") {
    JackAudioProcessor processor(1);

    // Signal exceeds 1.0 (clipping)
    auto clipped = generate_dc(256, 2.0f);
    const float* buffers[] = { clipped.data() };

    processor.process(256, buffers);
    auto meters = processor.get_meters();

    SECTION("RMS of 2.0 amplitude is ~+6 dB") {
        // 20 * log10(2.0) ≈ 6.02 dB
        REQUIRE_THAT(meters[0].rms_db, WithinAbs(6.0f, 0.1f));
    }

    SECTION("Peak of 2.0 amplitude is ~+6 dB") {
        REQUIRE_THAT(meters[0].peak_db, WithinAbs(6.0f, 0.1f));
    }
}

TEST_CASE("Jack | AudioProcessor - Thread safety", "[jack][audio_processor]") {
    // This is a basic test - true thread safety would require
    // running process() and get_meters() concurrently
    JackAudioProcessor processor(2);

    auto dc_ch0 = generate_dc(256, 0.5f);
    auto dc_ch1 = generate_dc(256, 0.5f);
    const float* buffers[] = { dc_ch0.data(), dc_ch1.data() };

    // Simulate multiple process cycles
    for (int i = 0; i < 100; ++i) {
        processor.process(256, buffers);
        auto meters = processor.get_meters();  // Concurrent read
        REQUIRE(meters.size() == 2);
        // Should not crash or produce garbage values
    }
}