//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_LIBJACK_CLIENT_API_H
#define AKNET_LIBJACK_CLIENT_API_H

#pragma once

#include "jack_interfaces.h"
#include <logger.h>
#include <jack/jack.h>
#include <memory>
#include <string>
#include <vector>

namespace aknet::jack {

    /**
     * Implementation of IJackClientAPI using libjack.
     *
     * Wraps JACK's C API (`jack/jack.h`) in a C++ interface.
     * Manages a single jack_client_t handle and registered ports.
     *
     * #### JACK C API Reference
     *
     * This class wraps the following JACK functions:
     * - `jack_client_open()`: Open connection to JACK server
     * - `jack_client_close()`: Close connection
     * - `jack_activate()`: Begin processing audio
     * - `jack_deactivate()`: Stop processing (not used currently)
     * - `jack_port_register()`: Create input/output ports
     * - `jack_port_unregister()`: Remove ports
     * - `jack_get_sample_rate()`: Query server sample rate
     * - `jack_get_buffer_size()`: Query server buffer size
     *
     * **Documentation**: [https://jackaudio.org/api/](https://jackaudio.org/api/)
     *
     * #### JACK Server Dependency
     *
     * All operations require a running JACK server.
     * If the server is not running, operations will fail.
     *
     * Use `JackNoStartServer` flag to prevent auto-starting server
     * (we manage server lifecycle separately via `JackServerManager`).
     *
     * #### Error Handling
     *
     * JACK C API uses several error reporting mechanisms:
     * - Return values: 0 = success, non-zero = failure
     * - Status flags: `jack_status_t` bitmask with detailed error info
     * - NULL returns: Indicate failure for functions returning pointers
     *
     * This wrapper translates all errors into `Result` objects with
     * descriptive error messages.
     *
     * #### Thread Safety
     *
     * NOT thread-safe. All methods should be called from the same thread (except the process callback, which runs on JACK's RT thread).
     *
     * JACK itself is thread-safe, but this wrapper maintains state that is not protected by mutexes.
     *
     * #### Resource Cleanup
     *
     * The destructor closes the JACK client if still open.
     * Ports are automatically unregistered when client closes.
     *
     * #### Platform Notes
     *
     * - **macOS**: JACK uses CoreAudio backend
     * - **Linux**: JACK uses ALSA backend
     *
     */
    class LibJackClientAPI : public IJackClientAPI {

    public:
        /**
         * Construct a LibJackClientAPI.
         *
         * @param logger Logger for diagnostic output.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         */
        explicit LibJackClientAPI(std::shared_ptr<log::Logger> logger);

        /**
         * Destructor.
         *
         * Closes the JACK client if still open.
         */
        ~LibJackClientAPI() override;

        // IJackClientAPI interface
        ServerInfo probe_server() override;

        Result open_client(const std::string& client_name) override;

        Result register_input_ports(int count) override;

        Result activate() override;

        Result close_client() override;

        bool is_active() const override;

        Result set_process_callback(JackProcessCallback callback, void* arg) override;

        const std::vector<jack_port_t*>& get_input_ports() const override;

    private:
        std::shared_ptr<log::Logger> logger_;

        jack_client_t* client_ = nullptr;
        bool is_active_ = false;

        // Track registered ports for cleanup
        std::vector<jack_port_t*> ports_;

        // Process callback state
        JackProcessCallback user_callback_;
        void* user_callback_arg_ = nullptr;

        // Static C callback wrapper for libjack
        static int process_callback_c_wrapper(jack_nframes_t nframes, void* arg);

        /**
         * Get JACK status string from status bits.
         *
         * Converts jack_status_t bitmask into human-readable string.
         * Handles all standard JACK status flags.
         *
         * @param status JACK status flags from jack_client_open().
         * @return Human-readable error description.
         */
        std::string jack_status_to_string(jack_status_t status);
    };

} // namespace aknet::jack

#endif //AKNET_LIBJACK_CLIENT_API_H