//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "jack_server_manager.h"
#include <stdexcept>

namespace aknet::jack {

    JackServerManager::JackServerManager(
        std::shared_ptr<log::Logger> logger,
        std::shared_ptr<IJackClientAPI> client_api,
        std::shared_ptr<IProcessRunner> process_runner)
        : logger_(std::move(logger))
        , client_api_(std::move(client_api))
        , process_runner_(std::move(process_runner))
    {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
        if (!client_api_) {
            throw std::invalid_argument("Client API cannot be null");
        }
        if (!process_runner_) {
            throw std::invalid_argument("Process runner cannot be null");
        }
    }

    JackServerManager::~JackServerManager() {
        // TODO: Stop owned server
    }

    ServerInfo JackServerManager::probe_server() {
        // TODO: Delegate to client_api_
        return {0, 0, false};
    }

    Result JackServerManager::ensure_server(const ServerConfig& config, bool force_restart) {
        // TODO: Implement server startup logic
        return {false, "Not implemented"};
    }

    Result JackServerManager::stop_server() {
        // TODO: Implement server stop logic
        return {true, ""};
    }

    bool JackServerManager::owns_server() const {
        return owned_server_pid_.has_value();
    }

} // namespace aknet::jack