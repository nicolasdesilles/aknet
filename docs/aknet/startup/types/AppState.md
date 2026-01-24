---
generator: doxide
---


# AppState

**enum class AppState**

Application lifecycle state.

Represents the high-level state of the application as it transitions through the startup process.


**Off**
:   Idle state, default when starting the app or after failure


**Booting**
:   Startup process is running


**Active**
:   App is fully active (audio engine, networking, etc.)


**ShuttingDown**
:   App is shutting down



