//
// Created by Nicolas Désilles on 27/12/2025.
//

#ifndef AKNET_CORE_H
#define AKNET_CORE_H

#pragma once
#include <logger.h>

namespace aknet {

    class core {

        public:

            ~core() {
                // Shutdown the logging system
                log::shutdown();
            }

            // We make these static since we will only have one core instance
            static void init();
            static void shutdown();

            static void test_function();

    private:
        static std::shared_ptr<spdlog::logger> logger;

    };

}

#endif //AKNET_CORE_H