//
// clock.h - Clock abstraction for testable time handling.
//

#ifndef AKNET_CLOCK_H
#define AKNET_CLOCK_H

#pragma once
#include <chrono>

namespace aknet::startup {

    namespace types {

        /**
         * Abstract interface for time access.
         *
         * Allows the startup engine to query current time through an interface that can be mocked for testing.
         */
        struct IClock {
            virtual ~IClock() = default;

            /**
             * Get the current time point.
             *
             * @return Current steady_clock time point.
             */
            virtual std::chrono::steady_clock::time_point now() const = 0;
        };

        /**
         * Real clock implementation using std::chrono::steady_clock.
         *
         * This is the default clock used in production.
         * steady_clock is preferred over system_clock because it is monotonic and not affected by system time changes.
         */
        struct SteadyClock : IClock {
            std::chrono::steady_clock::time_point now() const override {
                return std::chrono::steady_clock::now();
            }
        };

    } // namespace types

    // Re-export at startup:: level for API convenience
    using types::IClock;
    using types::SteadyClock;

}

#endif //AKNET_CLOCK_H
