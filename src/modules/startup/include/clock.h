//
// Created by Nicolas Désilles on 06/01/2026.
//

#ifndef AKNET_CLOCK_H
#define AKNET_CLOCK_H

#pragma once
#include <chrono>

namespace aknet::startup {

    struct IClock {
        virtual ~IClock() = default;
        virtual std::chrono::steady_clock::time_point now() const = 0;
    };

    struct SteadyClock : IClock {
        std::chrono::steady_clock::time_point now() const override {
            return std::chrono::steady_clock::now();
        }
    };
}

#endif //AKNET_CLOCK_H