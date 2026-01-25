//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_JACK_INTERFACES_H
#define AKNET_JACK_INTERFACES_H

#pragma once

#include <string>
#include <optional>
#include <functional>

namespace aknet::jack {

    /**
     * Result type for JACK operations.
     */
    struct Result {
        bool ok = true;
        std::string error;
    };

    /**
     * Information about a running JACK server.
     */
    struct ServerInfo {
        int sample_rate = 0;      ///< Server sample rate in Hz
        int buffer_size = 0;      ///< Server buffer size in frames
        bool is_running = false;  ///< True if server is reachable
    };

    /**
     * Abstract interface for spawning and managing external processes.
     *
     * Allows mocking process execution in tests.
     */
    class IProcessRunner {
    public:
        virtual ~IProcessRunner() = default;

        /**
         * Spawn a process and return immediately.
         *
         * @param executable Path to executable.
         * @param args Command-line arguments.
         * @param[out] pid Process ID of spawned child (if successful).
         *
         * @return Result indicating success or error.
         */
        virtual Result spawn(
            const std::string& executable,
            const std::vector<std::string>& args,
            int& pid
        ) = 0;

        /**
         * Check if a process is still running.
         *
         * @param pid Process ID to check.
         *
         * @return True if process is running.
         */
        virtual bool is_running(int pid) = 0;

        /**
         * Terminate a process.
         *
         * @param pid Process ID to terminate.
         * @param force If true, use SIGKILL; else SIGTERM.
         *
         * @return Result indicating success or error.
         */
        virtual Result terminate(int pid, bool force = false) = 0;
    };

    /**
     * Abstract interface for JACK client operations.
     *
     * Wraps libjack C API for testability.
     */
    class IJackClientAPI {
    public:
        virtual ~IJackClientAPI() = default;

        /**
         * Probe if a JACK server is running and get its info.
         *
         * Opens a temporary client, queries server, then closes.
         *
         * @return ServerInfo with is_running=true if reachable.
         */
        virtual ServerInfo probe_server() = 0;

        /**
         * Open a JACK client.
         *
         * @param client_name Name for the client.
         *
         * @return Result indicating success or error.
         */
        virtual Result open_client(const std::string& client_name) = 0;

        /**
         * Register input ports.
         *
         * @param count Number of mono input ports to register.
         *
         * @return Result indicating success or error.
         */
        virtual Result register_input_ports(int count) = 0;

        /**
         * Activate the JACK client.
         *
         * @return Result indicating success or error.
         */
        virtual Result activate() = 0;

        /**
         * Deactivate and close the JACK client.
         *
         * @return Result indicating success or error.
         */
        virtual Result close_client() = 0;

        /**
         * Check if client is currently open and active.
         *
         * @return True if client is active.
         */
        virtual bool is_active() const = 0;
    };

} // namespace aknet::jack

#endif //AKNET_JACK_INTERFACES_H