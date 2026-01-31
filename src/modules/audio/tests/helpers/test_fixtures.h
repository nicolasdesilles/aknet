//
// Created by Nicolas Désilles on 31/01/2026.
//

#ifndef AKNET_TEST_FIXTURES_H
#define AKNET_TEST_FIXTURES_H

#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <memory>

#include <logger.h>
#include <settings.h>

namespace aknet::test {

    /**
     * Base fixture for audio module tests.
     *
     * Provides logger and default settings.
     */
    struct AudioModuleTestFixture {
        std::shared_ptr<log::Logger> logger;
        settings::AppSettings settings;

        AudioModuleTestFixture() {
            // Create logger (writes to stderr in tests)
            log::init(".");
            log::set_global_log_level(log::LogLevel::debug);
            logger = log::get("audio_test");

            // Default test settings
            settings.audio.num_channels = 2;
            settings.audio.sampling_rate = 48000;
            settings.audio.buffer_size = 256;
        }

        ~AudioModuleTestFixture() {
            logger.reset();
        }
    };

} // namespace aknet::test

#endif //AKNET_TEST_FIXTURES_H