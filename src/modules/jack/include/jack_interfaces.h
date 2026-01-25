//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_JACK_INTERFACES_H
#define AKNET_JACK_INTERFACES_H

#pragma once

#include <string>
#include <functional>

#include <jack/jack.h>

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
     * Type definition for JACK process callback.
     *
     * Called by JACK's real-time audio thread for each process cycle.
     *
     * @param nframes Number of frames to process.
     * @param arg User-provided pointer passed to set_process_callback().
     *
     * @return 0 on success, non-zero to remove this client.
     */
    using JackProcessCallback = std::function<int(uint32_t nframes, void* arg)>;

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

        /**
         * Get registered input port handles.
         *
         * @return Vector of jack_port_t pointers. Empty if no ports registered.
         *
         * @note
         * These handles are needed to access audio buffers in the process callback.
         */
        virtual const std::vector<jack_port_t*>& get_input_ports() const = 0;

        /**
         * Set the process callback for the JACK client.
         *
         * The callback will be invoked by JACK's real-time thread for each process cycle.
         *
         * @param callback Function to call. Signature: int(uint32_t nframes, void* arg)
         * @param arg User pointer passed to callback on each invocation.
         *
         * @return Result indicating success or error.
         *
         * @note
         * Must be called BEFORE activate().
         */
        virtual Result set_process_callback(JackProcessCallback callback, void* arg) = 0;
    };

} // namespace aknet::jack

#endif //AKNET_JACK_INTERFACES_H