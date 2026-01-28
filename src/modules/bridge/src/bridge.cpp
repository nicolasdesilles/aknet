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

    void EventBridge::dispatch_event(const std::string& event_name, const json& detail) {
        std::string js = std::format(
            "window.dispatchEvent(new CustomEvent('{}', {{detail: {}}}));",
            event_name,
            detail.dump()
        );
        execute_fn_(js);
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
                    json j_progress = progress;
                    dispatch_event("startup:progress", j_progress);
                })
        );

        // StateChanged
        listener_ids_.push_back(
            manager->events().get<Event::StateChanged>()
                .add([this](AppState old_state, AppState new_state) {
                    json j = {
                        {"old_state", static_cast<int>(old_state)},
                        {"new_state", static_cast<int>(new_state)}
                    };
                    dispatch_event("startup:state", j);
                })
        );

        // StepStarted
        listener_ids_.push_back(
            manager->events().get<Event::StepStarted>()
                .add([this](int index, const std::string& id) {
                    json j = {{"index", index}, {"id", id}};
                    dispatch_event("startup:step_started", j);
                })
        );

        // StepCompleted
        listener_ids_.push_back(
            manager->events().get<Event::StepCompleted>()
                .add([this](int index, const std::string& id, StepStatus status) {
                    json j = {
                        {"index", index},
                        {"id", id},
                        {"status", static_cast<int>(status)}
                    };
                    dispatch_event("startup:step_completed", j);
                })
        );

        // SequenceCompleted
        listener_ids_.push_back(
            manager->events().get<Event::SequenceCompleted>()
                .add([this](bool success, const std::string& error_msg) {
                    json j = {{"success", success}, {"error", error_msg}};
                    dispatch_event("startup:completed", j);
                })
        );

        // Error
        listener_ids_.push_back(
            manager->events().get<Event::Error>()
                .add([this](const std::string& message) {
                    json j = {{"message", message}};
                    dispatch_event("startup:error", j);
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