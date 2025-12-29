//
// Created by Nicolas Désilles on 27/12/2025.
//

#ifndef AKNET_CORE_H
#define AKNET_CORE_H

#pragma once

#include <memory>
#include <filesystem>
#include <logger.h>

namespace aknet {

    struct core_config {
        std::filesystem::path log_dir = {};
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

    private:
        std::shared_ptr<log::Logger> logger_;

        // Future: owned modules
        // std::unique_ptr<ModuleA> module_a_;
    };

} // namespace aknet

#endif // AKNET_CORE_H