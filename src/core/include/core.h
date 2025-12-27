//
// Created by Nicolas Désilles on 27/12/2025.
//

#ifndef AKNET_CORE_H
#define AKNET_CORE_H

#pragma once
#include <memory>
#include <logger.h>

namespace aknet {

    class core {

        std::unique_ptr<logger::logger> logger_;

        public:
            void init();
            void shutdown();

            static void test_function();

    };

}

#endif //AKNET_CORE_H