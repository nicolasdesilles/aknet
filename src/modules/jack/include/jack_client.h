//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_JACK_CLIENT_H
#define AKNET_JACK_CLIENT_H

#pragma once

#include "jack_interfaces.h"
#include <jack_audio_processor.h>

#include <logger.h>
#include <memory>
#include <string>

namespace aknet::jack {

    /**
     * Configuration for JACK client
     */
    struct ClientConfig {
        std::string client_name;   ///< Name to register with JACK
        int input_port_count = 0;  ///< Number of input ports to create
    };

    /**
     * Client state
     */
    enum class ClientState {
        Closed,    ///< Client not opened
        Open,      ///< Client opened but not activated
        Active     ///< Client opened and activated
    };

    /**
     * Manages a JACK client for audio I/O.
     *
     * Responsibilities:
     * - Register client with JACK server
     * - Create input ports for audio capture
     * - Activate/deactivate client
     * - Manage client lifecycle
     *
     * #### Lifecycle
     *
     * 1. Construct → Closed
     * 2. open() → Open
     * 3. register_input_ports() → Open (with ports)
     * 4. activate() → Active
     * 5. close() → Closed
     *
     * #### Thread Safety
     *
     * NOT thread-safe. All methods should be called from the same thread
     * (except audio callback, which will run on JACK's RT thread).
     */
    class JackClient {

    public:
        /**
         * Construct a JackClient.
         *
         * @param logger Logger for diagnostic output.
         * @param client_api JACK client API interface.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         * - Throws `std::invalid_argument` if client_api is null.
         */
        JackClient(
            std::shared_ptr<log::Logger> logger,
            std::shared_ptr<IJackClientAPI> client_api);

        /**
         * Destructor.
         *
         * Closes the client if still open.
         */
        ~JackClient();

        // Non-copyable, non-movable
        JackClient(const JackClient&) = delete;
        JackClient& operator=(const JackClient&) = delete;

        /**
         * Open a JACK client with the given name.
         *
         * Must be called before register_input_ports() or activate().
         *
         * @param client_name Name to register with JACK server.
         *
         * @return Result indicating success or error.
         *
         * #### Errors
         *
         * - Client already open
         * - JACK server not running
         * - Client name already taken
         */
        Result open(const std::string& client_name);

        /**
         * Register input ports with JACK.
         *
         * Client must be open. Can be called multiple times,
         * but will fail if client is already active.
         *
         * @param count Number of input ports to create.
         *
         * @return Result indicating success or error.
         *
         * #### Errors
         *
         * - Client not open
         * - Client already active (ports can't change after activation)
         * - Invalid count (< 1)
         */
        Result register_input_ports(int count);

        /**
         * Activate the client to start processing audio.
         *
         * Client must be open. After activation, the audio callback
         * will be invoked on JACK's real-time thread.
         *
         * @return Result indicating success or error.
         *
         * #### Errors
         *
         * - Client not open
         * - Client already active
         */
        Result activate();

        /**
         * Close the client and clean up resources.
         *
         * Safe to call multiple times.
         *
         * @return Result indicating success or error.
         */
        Result close();

        /**
         * Check if client is active.
         *
         * @return True if client is activated.
         */
        bool is_active() const;

        /**
         * Get current client state.
         *
         * @return Current state (Closed, Open, or Active).
         */
        ClientState get_state() const;

        /**
         * Get the registered client name.
         *
         * @return Client name, or empty string if not open.
         */
        std::string get_client_name() const;

        /**
         * Get the number of registered input ports.
         *
         * @return Port count.
         */
        int get_input_port_count() const;

        /**
         * Set the audio processor for this client.
         *
         * The processor's process() method will be called from JACK's RT thread.
         *
         * @param processor Shared pointer to audio processor.
         *
         * @return Result indicating success or error.
         *
         * @note
         * Must be called BEFORE activate().
         */
        Result set_audio_processor(const std::shared_ptr<JackAudioProcessor>& processor);

        /**
         * Get current audio levels (thread-safe).
         *
         * @return Vector of meters, or empty if no processor set.
         */
        std::vector<ChannelMeter> get_audio_levels() const;

        /**
         * Reset peak hold meters (thread-safe).
         */
        void reset_peak_levels();

    private:
        std::shared_ptr<log::Logger> logger_;
        std::shared_ptr<IJackClientAPI> client_api_;

        ClientState state_ = ClientState::Closed;
        std::string client_name_;
        int input_port_count_ = 0;

        std::shared_ptr<JackAudioProcessor> audio_processor_;
        std::vector<jack_port_t*> input_ports_;  // Store for callback access

        // Instance callback
        int process_callback(jack_nframes_t nframes);
    };

} // namespace aknet::jack

#endif //AKNET_JACK_CLIENT_H