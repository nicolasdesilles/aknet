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

        auto result = client_api_->open_client(client_name);

        if (!result.ok) {
            return result;
        }

        state_ = ClientState::Open;
        client_name_ = client_name;

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

        auto result = client_api_->register_input_ports(count);

        if (!result.ok) {
            return result;
        }

        input_port_count_ = count;
        input_ports_ = client_api_->get_input_ports();

        // Pre-allocate RT buffer
        input_ptrs_.resize(input_ports_.size());

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

        auto result = client_api_->activate();

        if (!result.ok) {
            return result;
        }

        state_ = ClientState::Active;

        return result;
    }

    Result JackClient::close() {
        if (state_ == ClientState::Closed) {
            logger_->debug("Client already closed");
            return {true, ""};
        }

        auto result = client_api_->close_client();

        if (!result.ok) {
            return result;
        }

        state_ = ClientState::Closed;
        client_name_.clear();
        input_port_count_ = 0;
        input_ports_.clear();
        input_ptrs_.clear();
        audio_processor_.reset();

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

    Result JackClient::set_audio_processor(const std::shared_ptr<JackAudioProcessor>& processor) {
        if (state_ == ClientState::Active) {
            return { false, "Cannot set processor after activation" };
        }

        if (!processor) {
            return { false, "Processor cannot be null" };
        }

        audio_processor_ = processor;

        // Register callback with JACK API
        auto result = client_api_->set_process_callback(
            [this](uint32_t nframes, void* /*arg*/) {
                return this->process_callback(nframes);
            },
            this  // Pass 'this' as user arg
        );

        if (!result.ok) {
            audio_processor_.reset();
            return result;
        }

        return { true, "" };
    }

    int JackClient::process_callback(jack_nframes_t nframes) {
        if (!audio_processor_) {
            return 0;  // No-op if no processor (silence is okay)
        }

        // Validate we have ports
        if (input_ports_.empty()) {
            logger_->error("Process callback invoked with no ports registered");
            return 1;  // Error: deactivate client
        }

        // Fill pre-allocated buffer with pointers (RT-safe: no allocation)
        for (size_t i = 0; i < input_ports_.size(); ++i) {
            // jack_port_get_buffer() can return NULL if something is wrong
            void* buffer_raw = jack_port_get_buffer(input_ports_[i], nframes);
            if (!buffer_raw) {
                logger_->error("jack_port_get_buffer() returned NULL for port");
                return 1;  // Error: deactivate client
            }

            input_ptrs_[i] = static_cast<const float*>(buffer_raw);
        }

        // Process audio
        audio_processor_->process(nframes, input_ptrs_.data());

        return 0;  // Success
    }

    std::vector<ChannelMeter> JackClient::get_audio_levels() const {
        if (!audio_processor_) {
            return {};
        }
        return audio_processor_->get_meters();
    }

    void JackClient::reset_peak_levels() {
        if (audio_processor_) {
            audio_processor_->reset_peaks();
        }
    }

} // namespace aknet::jack