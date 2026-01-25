//
// Created by Nicolas Désilles on 25/01/2026.
//

#ifndef AKNET_POSIX_PROCESS_RUNNER_H
#define AKNET_POSIX_PROCESS_RUNNER_H

#pragma once

#include "jack_interfaces.h"
#include <logger.h>
#include <memory>
#include <string>
#include <vector>

namespace aknet::jack {
    /**
    * POSIX implementation of IProcessRunner.
    *
    * Spawns and manages child processes using fork() and exec().
    * Tracks PIDs to detect if processes are still running.
    *
    * #### Platform Support
    *
    * - macOS: Full support
    * - Linux: Full support
    * - Windows: **NOT SUPPORTED**
    *
    * @note
    * Windows does not support POSIX APIs (fork, execv, kill).
    * To add Windows support, we will later create a parallel `WindowsProcessRunner` class.
    */
    class PosixProcessRunner : public IProcessRunner {
    public:
        /**
         * Construct a PosixProcessRunner.
         *
         * @param logger Logger for diagnostics output
         *
         * #### Exceptions
         *
         * - Throws `std::invalid_argument if logger is null.
         */
        explicit PosixProcessRunner(std::shared_ptr<log::Logger> logger);

        /**
         * Destructor
         *
         * Does NOT terminate spawned processes automatically.
         */
        ~PosixProcessRunner() override = default;

        // IProcessRunner interface

        Result spawn(
            const std::string& executable,
            const std::vector<std::string>& args,
            int& pid
        ) override;

        bool is_running(int pid) override;

        Result terminate(int pid, bool force = false) override;

    private:
        std::shared_ptr<log::Logger> logger_;

        /**
        * Build argv array for execv().
        * Constructs a NULL-terminated array of C-strings suitable for execv().
        * The first element is the executable path, followed by arguments.
        *
        * @param executable Path to executable.
        * @param args Command-line arguments.
        * @return Vector of C-strings (char) with NULL terminator.
        */
        std::vector<char> build_argv(
            const std::string& executable,
            const std::vector<std::string>& args
        );

        /**
         * Wait for a zombie process to be reaped.
         * Non-blocking call using WNOHANG.
         * Called after terminate() to clean up zombie processes and avoid polluting process table.
         * If the process has exited, this reaps it.
         * If still running, returns immediately without blocking.
         *
         * @param pid Process ID to wait for.
         */
        void reap_zombie(int pid);

    };
}

#endif //AKNET_POSIX_PROCESS_RUNNER_H