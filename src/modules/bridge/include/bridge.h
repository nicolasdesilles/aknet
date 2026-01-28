//
// bridge.h - Event bridge for C++ to JavaScript communication.
//

#ifndef AKNET_EVENT_BRIDGE_H
#define AKNET_EVENT_BRIDGE_H

#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <functional>
#include <vector>
#include <utility>
#include <type_traits>

#include <saucer/webview.hpp>
#include <nlohmann/json.hpp>

namespace aknet::log {
    class Logger;
}

namespace aknet::startup::manager {
    class StartupManager;
}

namespace aknet::startup {
    using manager::StartupManager;
}

/**
 * Event bridge module for C++ to JavaScript communication.
 *
 * This module provides a bridge between the C++ backend and the JavaScript/React frontend,
 * forwarding events from various modules (like StartupManager) to the webview as DOM CustomEvents.
 *
 * The bridge uses the saucer webview library to execute JavaScript in the embedded browser.
 * Events are serialized to JSON and dispatched via `window.dispatchEvent()`.
 *
 * ## Design Goals
 *
 * - **Decoupling**: C++ modules don't need to know about the UI implementation
 * - **Type Safety**: Events are serialized using the existing JSON serialization infrastructure
 * - **Testability**: Template-based webview interface allows mocking for tests
 * - **Lifecycle Management**: Automatic cleanup of event listeners on disconnect/destruction
 *
 * ## Event Flow
 *
 * ```
 * StartupManager.events() → EventBridge → webview.execute(js) → window.dispatchEvent()
 * ```
 *
 * ## Frontend Integration
 *
 * The frontend listens for events using standard DOM event listeners:
 *
 * ```typescript
 * window.addEventListener('startup:progress', (e: CustomEvent) => {
 *     const progress = e.detail as SequenceProgress;
 *     // Update UI...
 * });
 * ```
 *
 * @see startup::StartupManager for the event source
 * @see startup::json for JSON serialization of event payloads
 */
namespace aknet::bridge {

    /**
     * Bridge for forwarding C++ events to the webview as JavaScript CustomEvents.
     *
     * EventBridge connects to event sources (like StartupManager) and forwards their events
     * to the embedded webview by executing JavaScript that dispatches DOM CustomEvents.
     *
     * #### Supported Events
     *
     * When connected to a StartupManager, the following events are forwarded:
     *
     * | C++ Event | JS Event Name | Payload |
     * |-----------|---------------|---------|
     * | ProgressChanged | `startup:progress` | Full SequenceProgress object |
     * | StateChanged | `startup:state` | `{old_state, new_state}` |
     * | StepStarted | `startup:step_started` | `{index, id}` |
     * | StepCompleted | `startup:step_completed` | `{index, id, status}` |
     * | SequenceCompleted | `startup:completed` | `{success, error}` |
     * | Error | `startup:error` | `{message}` |
     *
     * #### Thread Safety
     *
     * EventBridge is NOT thread-safe. It should be created and used from the main thread.
     * However, it safely handles events fired from worker threads (like StartupManager's worker).
     *
     * #### Lifecycle
     *
     * 1. Construct with a logger and webview pointer
     * 2. Call connect_startup_events() to start forwarding
     * 3. Events are automatically forwarded until disconnect() or destruction
     * 4. Destructor automatically disconnects all listeners
     *
     * #### Example
     *
     * ```cpp
     * // In Core initialization
     * auto bridge = std::make_unique<EventBridge>(logger, webview);
     * bridge->connect_startup_events(startup_manager);
     *
     * // Events are now forwarded to the webview
     * // Frontend can listen: window.addEventListener('startup:progress', ...)
     * ```
     *
     * @note The webview pointer must remain valid for the lifetime of the EventBridge.
     */
    class EventBridge {
    public:
        /**
         * Construct an EventBridge with a webview.
         *
         * The constructor is templated to support both the real saucer::webview
         * and mock implementations for testing.
         *
         * @tparam WebviewT Type with an `execute(const std::string&)` method.
         * @param logger Logger for diagnostic output. Must not be null.
         * @param webview Pointer to the webview. Must not be null.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         * - Throws `std::invalid_argument` if webview is null.
         *
         * #### Example
         *
         * ```cpp
         * auto logger = log::get("bridge");
         * auto bridge = std::make_unique<EventBridge>(logger, webview_ptr);
         * ```
         */
        template<typename WebviewT>
        EventBridge(std::shared_ptr<log::Logger> logger, WebviewT* webview);

        /**
         * Destructor.
         *
         * Automatically calls disconnect() to remove all event listeners.
         */
        ~EventBridge();

        // Non-copyable, non-movable
        EventBridge(const EventBridge&) = delete;
        EventBridge& operator=(const EventBridge&) = delete;
        EventBridge(EventBridge&&) = delete;
        EventBridge& operator=(EventBridge&&) = delete;

        /**
         * Connect to StartupManager events.
         *
         * Subscribes to all StartupManager events and forwards them to the webview
         * as JavaScript CustomEvents.
         *
         * If already connected to a manager, disconnects first before connecting to the new one.
         *
         * @param manager The StartupManager to connect to. Must not be null.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if manager is null.
         *
         * #### Events Forwarded
         *
         * - `startup:progress` - Full progress snapshot after each step
         * - `startup:state` - State transitions (Off → Booting → Active)
         * - `startup:step_started` - When a step begins execution
         * - `startup:step_completed` - When a step finishes (with status)
         * - `startup:completed` - When the entire sequence finishes
         * - `startup:error` - Critical errors
         *
         * #### Example
         *
         * ```cpp
         * bridge->connect_startup_events(startup_manager_ptr);
         *
         * // In React:
         * // useEffect(() => {
         * //     window.addEventListener('startup:progress', handleProgress);
         * //     return () => window.removeEventListener('startup:progress', handleProgress);
         * // }, []);
         * ```
         */
        void connect_startup_events(std::shared_ptr<startup::StartupManager> manager);

        /**
         * Dispatch a custom event to the webview.
         *
         * @param event_name Name of the JS CustomEvent to dispatch.
         * @param detail JSON payload to send in event detail.
         */
        void dispatch_event(const std::string& event_name, const nlohmann::json& detail);

        /**
         * Disconnect from all connected event sources.
         *
         * Removes all event listeners that were added by connect_*_events() methods.
         * Safe to call multiple times or when not connected.
         *
         * @note
         * Called automatically by the destructor.
         */
        void disconnect();

    private:
        std::shared_ptr<log::Logger> logger_;
        std::function<void(const std::string&)> execute_fn_;

        // Store manager + listener IDs for cleanup
        std::shared_ptr<startup::StartupManager> connected_manager_;
        std::vector<std::size_t> listener_ids_;
    };

    template<typename WebviewT>
    EventBridge::EventBridge(std::shared_ptr<log::Logger> logger, WebviewT* webview)
        : logger_(std::move(logger))
    {
        if (!logger_) {
            throw std::invalid_argument("EventBridge requires non-null logger");
        }
        if (!webview) {
            throw std::invalid_argument("EventBridge requires non-null webview");
        }

        // Use SFINAE: cast to saucer::webview if it's derived from it, otherwise call directly
        if constexpr (std::is_base_of_v<saucer::webview, WebviewT>) {
            execute_fn_ = [webview](const std::string& js) {
                static_cast<saucer::webview*>(webview)->execute(js);
            };
        } else {
            // For testing with MockWebview
            execute_fn_ = [webview](const std::string& js) {
                webview->execute(js);
            };
        }
    }

}

#endif //AKNET_EVENT_BRIDGE_H