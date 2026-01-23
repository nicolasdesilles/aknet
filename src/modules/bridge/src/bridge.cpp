//
// Created by Nicolas Désilles on 14/01/2026.
//

#include "bridge.h"
#include <logger.h>
#include <startup_manager.h>
#include <startup_json.h>
#include <nlohmann/json.hpp>
#include <array>

namespace aknet::bridge {

    using json = nlohmann::json;

    // Destructor

    EventBridge::~EventBridge() {
        disconnect();
    }

    void EventBridge::connect_startup_events(
        std::shared_ptr<startup::StartupManager> manager)
    {
        if (!manager) {
            throw std::invalid_argument("StartupManager cannot be null");
        }

        disconnect();

        connected_manager_ = manager;

        using namespace startup;
        using Event = StartupManager::Event;

        // ProgressChanged
        listener_ids_.push_back(
            manager->events().get<Event::ProgressChanged>()
                .add([this](const SequenceProgress& progress) {
                    try {
                        json j_progress = progress;
                        std::string js = std::format(
                            "window.dispatchEvent(new CustomEvent('startup:progress', "
                            "{{detail: {}}}));",
                            j_progress.dump()
                        );
                        execute_fn_(js);
                    } catch (const std::exception& e) {
                        logger_->error("Failed to dispatch ProgressChanged: {}", e.what());
                    }
                })
        );

        // StateChanged
        listener_ids_.push_back(
            manager->events().get<Event::StateChanged>()
                .add([this](AppState old_state, AppState new_state) {
                    try {
                        json j = {
                            {"old_state", static_cast<int>(old_state)},
                            {"new_state", static_cast<int>(new_state)}
                        };
                        std::string js = std::format(
                            "window.dispatchEvent(new CustomEvent('startup:state', "
                            "{{detail: {}}}));",
                            j.dump()
                        );
                        execute_fn_(js);
                    } catch (const std::exception& e) {
                        logger_->error("Failed to dispatch StateChanged: {}", e.what());
                    }
                })
        );

        // StepStarted
        listener_ids_.push_back(
            manager->events().get<Event::StepStarted>()
                .add([this](int index, const std::string& id) {
                    try {
                        json j = {{"index", index}, {"id", id}};
                        std::string js = std::format(
                            "window.dispatchEvent(new CustomEvent('startup:step_started', "
                            "{{detail: {}}}));",
                            j.dump()
                        );
                        execute_fn_(js);
                    } catch (const std::exception& e) {
                        logger_->error("Failed to dispatch StepStarted: {}", e.what());
                    }
                })
        );

        // StepCompleted
        listener_ids_.push_back(
            manager->events().get<Event::StepCompleted>()
                .add([this](int index, const std::string& id, StepStatus status) {
                    try {
                        json j = {
                            {"index", index},
                            {"id", id},
                            {"status", static_cast<int>(status)}
                        };
                        std::string js = std::format(
                            "window.dispatchEvent(new CustomEvent('startup:step_completed', "
                            "{{detail: {}}}));",
                            j.dump()
                        );
                        execute_fn_(js);
                    } catch (const std::exception& e) {
                        logger_->error("Failed to dispatch StepCompleted: {}", e.what());
                    }
                })
        );

        // SequenceCompleted
        listener_ids_.push_back(
            manager->events().get<Event::SequenceCompleted>()
                .add([this](bool success, const std::string& error_msg) {
                    try {
                        json j = {{"success", success}, {"error", error_msg}};
                        std::string js = std::format(
                            "window.dispatchEvent(new CustomEvent('startup:completed', "
                            "{{detail: {}}}));",
                            j.dump()
                        );
                        execute_fn_(js);
                    } catch (const std::exception& e) {
                        logger_->error("Failed to dispatch SequenceCompleted: {}", e.what());
                    }
                })
        );

        // Error
        listener_ids_.push_back(
            manager->events().get<Event::Error>()
                .add([this](const std::string& message) {
                    try {
                        json j = {{"message", message}};
                        std::string js = std::format(
                            "window.dispatchEvent(new CustomEvent('startup:error', "
                            "{{detail: {}}}));",
                            j.dump()
                        );
                        execute_fn_(js);
                    } catch (const std::exception& e) {
                        logger_->error("Failed to dispatch Error: {}", e.what());
                    }
                })
        );
    }

    void EventBridge::disconnect() {
        if (!connected_manager_ || listener_ids_.empty()) {
            return;
        }

        using Event = startup::StartupManager::Event;

        // Remove listeners in the same order they were added
        // Order: ProgressChanged, StateChanged, StepStarted, StepCompleted, SequenceCompleted, Error
        const std::array<Event, 6> event_order = {
            Event::ProgressChanged,
            Event::StateChanged,
            Event::StepStarted,
            Event::StepCompleted,
            Event::SequenceCompleted,
            Event::Error
        };

        for (size_t i = 0; i < listener_ids_.size() && i < event_order.size(); ++i) {
            switch (event_order[i]) {
                case Event::ProgressChanged:
                    connected_manager_->events().get<Event::ProgressChanged>().remove(listener_ids_[i]);
                    break;
                case Event::StateChanged:
                    connected_manager_->events().get<Event::StateChanged>().remove(listener_ids_[i]);
                    break;
                case Event::StepStarted:
                    connected_manager_->events().get<Event::StepStarted>().remove(listener_ids_[i]);
                    break;
                case Event::StepCompleted:
                    connected_manager_->events().get<Event::StepCompleted>().remove(listener_ids_[i]);
                    break;
                case Event::SequenceCompleted:
                    connected_manager_->events().get<Event::SequenceCompleted>().remove(listener_ids_[i]);
                    break;
                case Event::Error:
                    connected_manager_->events().get<Event::Error>().remove(listener_ids_[i]);
                    break;
            }
        }

        listener_ids_.clear();
        connected_manager_.reset();
    }

}