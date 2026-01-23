//
// Created by Nicolas Désilles on 14/01/2026.
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

namespace aknet::log {
    class Logger;
}

namespace aknet::startup {
    class StartupManager;
}

namespace aknet::bridge {

    class EventBridge {
    public:
        // Constructor
        template<typename WebviewT>
        EventBridge(std::shared_ptr<log::Logger> logger,
                    WebviewT* webview);

        // Destructor
        ~EventBridge();

        // non-copyable, non-movable
        EventBridge(const EventBridge&) = delete;
        EventBridge& operator=(const EventBridge&) = delete;
        EventBridge(EventBridge&&) = delete;
        EventBridge& operator=(EventBridge&&) = delete;

        void connect_startup_events(std::shared_ptr<startup::StartupManager> manager);
        void disconnect();

    private:
        std::shared_ptr<log::Logger> logger_;
        std::function<void(const std::string&)> execute_fn_;

        // Store manager + listener IDs for cleanup
        std::shared_ptr<startup::StartupManager> connected_manager_;
        std::vector<std::size_t> listener_ids_;
    };

    template<typename WebviewT>
    EventBridge::EventBridge(std::shared_ptr<log::Logger> logger,
                             WebviewT* webview)
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