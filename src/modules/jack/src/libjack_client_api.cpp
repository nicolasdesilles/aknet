//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "libjack_client_api.h"
#include <stdexcept>
#include <sstream>

namespace aknet::jack {

    LibJackClientAPI::LibJackClientAPI(std::shared_ptr<log::Logger> logger)
        : logger_(std::move(logger))
        , user_callback_(nullptr)
    {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
    }

    LibJackClientAPI::~LibJackClientAPI() {
        if (client_) {
            logger_->debug("Closing JACK client in destructor");
            LibJackClientAPI::close_client();
        }
    }

    ServerInfo LibJackClientAPI::probe_server() {
        // Open a temporary client to query server
        // JackNoStartServer: Don't auto-start server if not running
        jack_status_t status;
        jack_client_t* temp_client = jack_client_open(
            "aknet_probe",
            JackNoStartServer,
            &status
        );

        if (!temp_client) {
            // Debug level since this is expected during polling/startup
            logger_->debug("JACK server not reachable (status: {})", jack_status_to_string(status));
            return {0, 0, false};
        }

        // Query server info
        ServerInfo info;
        info.sample_rate = static_cast<int>(jack_get_sample_rate(temp_client));
        info.buffer_size = static_cast<int>(jack_get_buffer_size(temp_client));
        info.is_running = true;

        // Close temporary client
        jack_client_close(temp_client);

        return info;
    }

    Result LibJackClientAPI::open_client(const std::string& client_name) {
        if (client_) {
            logger_->warn("Client already open");
            return {false, "Client already open"};
        }

        if (client_name.empty()) {
            logger_->error("Client name cannot be empty");
            return {false, "Client name cannot be empty"};
        }

        logger_->info("Opening JACK client: {}", client_name);

        // Open JACK client
        // JackNoStartServer: Don't auto-start (we manage server via JackServerManager)
        jack_status_t status;
        client_ = jack_client_open(
            client_name.c_str(),
            JackNoStartServer,
            &status
        );

        if (!client_) {
            std::string error = "Failed to open JACK client: " + jack_status_to_string(status);
            logger_->error("{}", error);
            return {false, error};
        }

        // Check if name was modified by server
        if (status & JackNameNotUnique) {
            const char* actual_name = jack_get_client_name(client_);
            logger_->warn("Requested name '{}' was taken, using '{}'",
                          client_name, actual_name);
        }

        logger_->info("JACK client opened successfully");
        return {true, ""};
    }

    Result LibJackClientAPI::register_input_ports(int count) {
        if (!client_) {
            logger_->error("Cannot register ports: client not open");
            return {false, "Client not open"};
        }

        if (is_active_) {
            logger_->error("Cannot register ports: client already active");
            return {false, "Cannot register ports after activation"};
        }

        if (count < 1) {
            logger_->error("Invalid port count: {}", count);
            return {false, "Invalid port count"};
        }

        logger_->info("Registering {} input ports", count);

        // Unregister existing ports if any
        for (auto* port : ports_) {
            jack_port_unregister(client_, port);
        }
        ports_.clear();

        // Register new ports
        for (int i = 0; i < count; ++i) {
            std::ostringstream port_name;
            port_name << "input_" << (i + 1);

            // jack_port_register() parameters:
            // - client: JACK client handle
            // - port_name: Short name (full name will be "client_name:port_name")
            // - port_type: JACK_DEFAULT_AUDIO_TYPE for audio ports
            // - flags: JackPortIsInput for input ports
            // - buffer_size: 0 for audio (uses server buffer size)
            jack_port_t* port = jack_port_register(
                client_,
                port_name.str().c_str(),
                JACK_DEFAULT_AUDIO_TYPE,
                JackPortIsInput,
                0
            );

            if (!port) {
                std::ostringstream error;
                error << "Failed to register port " << port_name.str();
                logger_->error("{}", error.str());

                // Clean up already-registered ports
                for (auto* p : ports_) {
                    jack_port_unregister(client_, p);
                }
                ports_.clear();

                return {false, error.str()};
            }

            ports_.push_back(port);
            logger_->debug("Registered port: {}", port_name.str());
        }

        logger_->info("Successfully registered {} input ports", count);
        return {true, ""};
    }

    Result LibJackClientAPI::activate() {
        if (!client_) {
            logger_->error("Cannot activate: client not open");
            return {false, "Client not open"};
        }

        if (is_active_) {
            logger_->warn("Client already active");
            return {false, "Client already active"};
        }

        logger_->info("Activating JACK client");

        int result = jack_activate(client_);

        if (result != 0) {
            std::string error = "jack_activate() failed";
            logger_->error("{}", error);
            return {false, error};
        }

        is_active_ = true;
        logger_->info("JACK client activated");
        return {true, ""};
    }

    Result LibJackClientAPI::close_client() {
        if (!client_) {
            logger_->debug("Client already closed");
            return {true, ""};
        }

        logger_->info("Closing JACK client");

        // Ports are automatically unregistered by jack_client_close()
        // Clear our tracking vector
        ports_.clear();

        // Close client connection
        int result = jack_client_close(client_);

        if (result != 0) {
            std::string error = "jack_client_close() failed";
            logger_->error("{}", error);
            // Still mark as closed to avoid re-closing
            client_ = nullptr;
            is_active_ = false;
            return {false, error};
        }

        client_ = nullptr;
        is_active_ = false;
        logger_->info("JACK client closed");
        return {true, ""};
    }

    bool LibJackClientAPI::is_active() const {
        return is_active_;
    }

    const std::vector<jack_port_t*>& LibJackClientAPI::get_input_ports() const {
        return ports_;
    }

    Result LibJackClientAPI::set_process_callback(JackProcessCallback callback, void* arg) {
        if (!client_) {
            logger_->error("Cannot set callback: client not open");
            return {false, "Client not open"};
        }

        if (is_active_) {
            logger_->error("Cannot set callback: client already active");
            return {false, "Cannot set callback after activation"};
        }

        logger_->debug("Setting process callback");

        user_callback_ = callback;
        user_callback_arg_ = arg;

        // Register C callback with libjack
        int result = jack_set_process_callback(client_, process_callback_c_wrapper, this);
        if (result != 0) {
            std::string error = "jack_set_process_callback() failed";
            logger_->error("{}", error);
            user_callback_ = nullptr;
            user_callback_arg_ = nullptr;
            return {false, error};
        }

        logger_->debug("Process callback registered successfully");
        return {true, ""};
    }

    int LibJackClientAPI::process_callback_c_wrapper(jack_nframes_t nframes, void* arg) {
        auto* self = static_cast<LibJackClientAPI*>(arg);
        if (self && self->user_callback_) {
            return self->user_callback_(nframes, self->user_callback_arg_);
        }
        return 0;  // No-op if no callback set
    }

    std::string LibJackClientAPI::jack_status_to_string(jack_status_t status) {
        if (status == 0) {
            return "Success";
        }

        std::ostringstream oss;

        // Decode all possible status flags
        // See jack/types.h for definitions
        if (status & JackFailure)         oss << "JackFailure ";
        if (status & JackInvalidOption)   oss << "JackInvalidOption ";
        if (status & JackNameNotUnique)   oss << "JackNameNotUnique ";
        if (status & JackServerStarted)   oss << "JackServerStarted ";
        if (status & JackServerFailed)    oss << "JackServerFailed ";
        if (status & JackServerError)     oss << "JackServerError ";
        if (status & JackNoSuchClient)    oss << "JackNoSuchClient ";
        if (status & JackLoadFailure)     oss << "JackLoadFailure ";
        if (status & JackInitFailure)     oss << "JackInitFailure ";
        if (status & JackShmFailure)      oss << "JackShmFailure ";
        if (status & JackVersionError)    oss << "JackVersionError ";
        if (status & JackBackendError)    oss << "JackBackendError ";
        if (status & JackClientZombie)    oss << "JackClientZombie ";

        return oss.str();
    }

} // namespace aknet::jack