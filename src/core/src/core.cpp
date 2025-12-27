//
// Created by Nicolas Désilles on 27/12/2025.
//

#include "core.h"
#include "version.h"
#include <iostream>

namespace aknet {
    void core::init() {

        logger_ = logger::create_logger();
        logger_->init();

        LOG_INFO("aknet - v{0} by {1}", PROJECT_VERSION, PROJECT_AUTHOR);

    }

    void core::shutdown() {
        if (logger_) {
            logger_->shutdown();
            logger_.reset();
        }
    }

    void core::test_function() {
        LOG_INFO("Core test function called");
    }
}
