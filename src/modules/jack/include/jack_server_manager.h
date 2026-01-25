//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_JACK_SERVER_MANAGER_H
#define AKNET_JACK_SERVER_MANAGER_H

#pragma once

#include "jack_interfaces.h"
#include <logger.h>
#include <memory>
#include <optional>

namespace aknet::jack {
    /**
     * Configuration for JACK server management
     */
    struct ServerConfig {
        std::string executable_path;   ///< Path to jackd executable
        int sample_rate = 48000;       ///< Target sample rate
        int buffer_size = 256;         ///< Target buffer size
    };

    /**
     * Manages the lifecycle of a JACK server process.
     *
     * Responsibilities:
     * - Start jackd with specified settings
     * - Stop owned jackd process
     * - Detect if server is already running
     * - Query running server settings
     *
     * #### Ownership model
     *
     * The manager only controls servers it started itself.
     * If a server is already running and wasn't started by this instance,
     * operations that would restart it will fail (unless forced).
     *
     * #### Thread Safety
     *
     * NOT thread-safe. All methods should be called from the same thread.
     */
    class JackServerManager {

    public:
        /**
         * Construct a JackServerManager.
         *
         * @param logger Logger for diagnostic output.
         * @param client_api JACK client API for probing server.
         * @param process_runner Process spawning interface.
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument` if logger is null.
         * - Throws `std::invalid_argument` if client_api is null.
         * - Throws `std::invalid_argument` if process_runner is null.
         */
        JackServerManager(
            std::shared_ptr<log::Logger> logger,
            std::shared_ptr<IJackClientAPI> client_api,
            std::shared_ptr<IProcessRunner> process_runner);

        /**
         * Destructor.
         *
         * Stops the owned server process if running.
         */
        ~JackServerManager();

        // Non-copyable, non-movable
        JackServerManager(const JackServerManager&) = delete;
        JackServerManager& operator=(const JackServerManager&) = delete;

        /**
         * Probe if a JACK server is running.
         *
         * @return ServerInfo with is_running and settings if reachable.
         */
        ServerInfo probe_server();

        /**
         * Ensure a JACK server is running with the specified config.
         *
         * Policy:
         * - If no server is running: start one
         * - If server is running with correct settings: do nothing
         * - If server is running with wrong settings and owned by aknet: restart
         * - If server is running with wrong settings and NOT owned by aknet: fail (unless force=true)
         *
         * @param config Desired server configuration.
         * @param force_restart If true, restart external servers without confirmation.
         *
         * @return Result indicating success or error.
         */
        Result ensure_server(const ServerConfig& config, bool force_restart = false);

        /**
         * Stop the server if we own it.
         *
         * No-op if server wasn't started by this manager.
         *
         * @return Result indicating success or error.
         */
        Result stop_server();

        /**
         * Check if this manager owns the running server.
         *
         * @return True if we started the server.
         */
        bool owns_server() const;

    private:
        std::shared_ptr<log::Logger> logger_;
        std::shared_ptr<IJackClientAPI> client_api_;
        std::shared_ptr<IProcessRunner> process_runner_;

        /**
         * PID if we started the server
        */
        std::optional<int> owned_server_pid_;

    };

}

#endif //AKNET_JACK_SERVER_MANAGER_H