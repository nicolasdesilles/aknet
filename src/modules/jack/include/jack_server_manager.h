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

#include "audio_device_manager.h"

namespace aknet::jack {
    /**
     * Configuration for JACK server management
     */
    struct ServerConfig {
        std::string executable_path;   ///< Path to jackd executable
        int sample_rate = 48000;       ///< Target sample rate
        int buffer_size = 256;         ///< Target buffer size
        std::string input_device_id = "system_default";     ///< Input device ID. Use "system_default" for driver default.
        std::string output_device_id = "system_default";    ///< Output device ID. Use "system_default" for driver default.
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
            std::shared_ptr<IAudioDeviceManager> device_manager = nullptr;

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


        /**
         * Validate that the configured devices exist and are usable.
         *
         * Checks that:
         * - Device IDs resolve to actual devices (or are "system_default")
         * - Input device has input channels
         * - Output device has output channels
         *
         * @param config Configuration to validate.
         * @return Result indicating success or validation error.
         */
        Result validate_device_config(const ServerConfig& config);

    private:
        std::shared_ptr<log::Logger> logger_;
        std::shared_ptr<IJackClientAPI> client_api_;
        std::shared_ptr<IProcessRunner> process_runner_;
        std::shared_ptr<IAudioDeviceManager> device_manager_;

        /**
         * PID if we started the server
        */
        std::optional<int> owned_server_pid_;

        /**
         * Configuration used to start the owned server.
         *
         * Only valid when owned_server_pid_ is set.
         * Used to detect if config has changed and restart is needed.
         */
        std::optional<ServerConfig> last_server_config_;

        /**
         * Wait for the JACK server to be ready to accept connections.
         *
         * Polls the server with a timeout. Used after spawning jackd
         * since the server takes time to initialize before accepting clients.
         *
         * @return Result indicating success or timeout error.
         */
        Result wait_for_server_ready();

        /**
         * Build jackd command-line arguments from configuration.
         *
         * Constructs the argument vector for spawning jackd, including:
         * - Realtime flag (-R)
         * - CoreAudio driver selection (-d coreaudio)
         * - Sample rate (-r)
         * - Buffer size (-p)
         * - Input device (-C) if not system_default
         * - Output device (-P) if not system_default
         *
         * @param config Server configuration.
         * @return Vector of command-line arguments.
         */
        std::vector<std::string> build_jackd_args(const ServerConfig& config);

    };

}

#endif //AKNET_JACK_SERVER_MANAGER_H