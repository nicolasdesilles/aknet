---
generator: doxide
---


# SteadyClock

**struct SteadyClock : IClock**

Get the current time point.


:material-keyboard-return: **Return**
:    Current steady_clock time point.
Real clock implementation using std::chrono::steady_clock.

This is the default clock used in production.
steady_clock is preferred over system_clock because it is monotonic and not affected by system time changes.


