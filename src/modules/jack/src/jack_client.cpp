//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "jack_client.h"
#include <stdexcept>

namespace aknet::jack {

JackClient::JackClient(
    std::shared_ptr<log::Logger> logger,
    std::shared_ptr<IJackClientAPI> client_api)
    : logger_(std::move(logger))
    , client_api_(std::move(client_api))
{
    if (!logger_) {
        throw std::invalid_argument("Logger cannot be null");
    }
    if (!client_api_) {
        throw std::invalid_argument("Client API cannot be null");
    }
}

JackClient::~JackClient() {
    close();
}

Result JackClient::open(const std::string& client_name) {
    if (state_ != ClientState::Closed) {
        logger_->warn("Attempted to open client when already open");
        return {false, "Client already open"};
    }

    if (client_name.empty()) {
        logger_->error("Cannot open client with empty name");
        return {false, "Client name cannot be empty"};
    }

    logger_->info("Opening JACK client: {}", client_name);

    auto result = client_api_->open_client(client_name);

    if (!result.ok) {
        logger_->error("Failed to open JACK client: {}", result.error);
        return result;
    }

    state_ = ClientState::Open;
    client_name_ = client_name;
    logger_->info("JACK client opened successfully");

    return result;
}

Result JackClient::register_input_ports(int count) {
    if (state_ == ClientState::Closed) {
        logger_->error("Cannot register ports: client not open");
        return {false, "Client not open"};
    }

    if (state_ == ClientState::Active) {
        logger_->error("Cannot register ports: client already active");
        return {false, "Cannot change ports after client is active"};
    }

    if (count < 1) {
        logger_->error("Invalid port count: {}", count);
        return {false, "Invalid port count"};
    }

    logger_->info("Registering {} input ports", count);

    auto result = client_api_->register_input_ports(count);

    if (!result.ok) {
        logger_->error("Failed to register input ports: {}", result.error);
        return result;
    }

    input_port_count_ = count;
    logger_->info("Registered {} input ports", count);

    return result;
}

Result JackClient::activate() {
    if (state_ == ClientState::Closed) {
        logger_->error("Cannot activate: client not open");
        return {false, "Client not open"};
    }

    if (state_ == ClientState::Active) {
        logger_->warn("Client already active");
        return {false, "Client already active"};
    }

    logger_->info("Activating JACK client");

    auto result = client_api_->activate();

    if (!result.ok) {
        logger_->error("Failed to activate JACK client: {}", result.error);
        return result;
    }

    state_ = ClientState::Active;
    logger_->info("JACK client activated");

    return result;
}

Result JackClient::close() {
    if (state_ == ClientState::Closed) {
        logger_->debug("Client already closed");
        return {true, ""};
    }

    logger_->info("Closing JACK client");

    auto result = client_api_->close_client();

    if (!result.ok) {
        logger_->error("Failed to close JACK client: {}", result.error);
        return result;
    }

    state_ = ClientState::Closed;
    client_name_.clear();
    input_port_count_ = 0;
    logger_->info("JACK client closed");

    return result;
}

bool JackClient::is_active() const {
    return state_ == ClientState::Active;
}

ClientState JackClient::get_state() const {
    return state_;
}

std::string JackClient::get_client_name() const {
    return client_name_;
}

int JackClient::get_input_port_count() const {
    return input_port_count_;
}

} // namespace aknet::jack