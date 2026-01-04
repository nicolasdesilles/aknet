//
// Created by Nicolas Désilles on 27/12/2025.
//

#ifndef AKNET_CORE_H
#define AKNET_CORE_H

#pragma once

#include <memory>
#include <filesystem>

// aknet utils and modules
#include <logger.h>
#include <settings.h>

namespace aknet {

    struct core_config {
        std::filesystem::path log_dir = {};
        std::filesystem::path settings_dir = {};
        int settings_schema_version = 1;
        log::LogLevel log_level = log::LogLevel::info;
    };

    class core {
    public:

        explicit core(const core_config& config = {});
        ~core();

        // Non-copyable, non-movable
        core(const core&) = delete;
        core& operator=(const core&) = delete;

        void test_function();

        // Future: accessors for owned modules
        // ModuleA& module_a();

        settings::Settings settings_;

    private:
        std::shared_ptr<log::Logger> logger_;

        void log_aknet_start_message();

        // Future: owned modules
        // std::unique_ptr<ModuleA> module_a_;
    };

} // namespace aknet

#endif // AKNET_CORE_H