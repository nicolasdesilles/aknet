//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "posix_process_runner.h"

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sstream>

namespace aknet::jack {

PosixProcessRunner::PosixProcessRunner(std::shared_ptr<log::Logger> logger)
    : logger_(std::move(logger))
{
    if (!logger_) {
        throw std::invalid_argument("Logger cannot be null");
    }
}

Result PosixProcessRunner::spawn(
    const std::string& executable,
    const std::vector<std::string>& args,
    int& pid)
{
    logger_->info("Spawning process: {} with {} args", executable, args.size());

    // Verify executable exists and is executable
    if (access(executable.c_str(), X_OK) != 0) {
        std::string error = "Executable not found or not executable: " + executable;
        logger_->error("{}", error);
        return {false, error};
    }

    // Fork child process
    pid_t child_pid = fork();

    if (child_pid < 0) {
        // Fork failed
        std::string error = std::string("fork() failed: ") + strerror(errno);
        logger_->error("{}", error);
        return {false, error};
    }

    if (child_pid == 0) {
        // CHILD PROCESS

        // Build argv for execv()
        std::vector<std::string> argv_strings;
        argv_strings.push_back(executable);
        argv_strings.insert(argv_strings.end(), args.begin(), args.end());

        std::vector<char*> argv;
        for (auto& str : argv_strings) {
            argv.push_back(const_cast<char*>(str.c_str()));
        }
        argv.push_back(nullptr);  // execv requires NULL-terminated array

        // Replace child process image with executable
        execv(executable.c_str(), argv.data());

        // If execv returns, it failed
        // IMPORTANT: Use _exit() not exit() to avoid flushing parent's buffers
        _exit(127);
    }

    // PARENT PROCESS

    pid = child_pid;
    logger_->info("Spawned process with PID {}", pid);

    // Give child a moment to start (or fail immediately)
    // Check if child died immediately (e.g., bad executable path)
    usleep(10000);  // 10ms

    int status;
    pid_t result = waitpid(child_pid, &status, WNOHANG);

    if (result == child_pid) {
        // Child already exited
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            std::ostringstream oss;
            oss << "Process exited immediately with code " << exit_code;
            logger_->error("{}", oss.str());
            return {false, oss.str()};
        }

        if (WIFSIGNALED(status)) {
            int signal = WTERMSIG(status);
            std::ostringstream oss;
            oss << "Process killed by signal " << signal;
            logger_->error("{}", oss.str());
            return {false, oss.str()};
        }
    }

    return {true, ""};
}

bool PosixProcessRunner::is_running(int pid) {
    if (pid <= 0) {
        return false;
    }

    // Use kill(pid, 0) to check if process exists
    // Signal 0 is a special "null signal" that doesn't actually send anything, but performs error checking as if a signal would be sent.
    // Returns:
    // - 0: Process exists and we have permission to signal it
    // - -1 with ESRCH: Process doesn't exist
    // - -1 with EPERM: Process exists but we don't have permission
    int result = kill(pid, 0);

    if (result == 0) {
        return true;  // Process exists
    }

    if (errno == ESRCH) {
        return false;  // Process doesn't exist
    }

    // Other errors (EPERM, EINVAL) - assume running
    // EPERM means process exists but we can't signal it
    logger_->warn("kill(pid={}, 0) returned error: {}", pid, strerror(errno));
    return true;
}

Result PosixProcessRunner::terminate(int pid, bool force) {
    if (pid <= 0) {
        return {false, "Invalid PID"};
    }

    // Choose signal based on force flag
    // SIGTERM (15): Graceful termination, allows cleanup
    // SIGKILL (9): Immediate termination, cannot be caught or ignored
    int signal = force ? SIGKILL : SIGTERM;
    const char* signal_name = force ? "SIGKILL" : "SIGTERM";

    logger_->info("Sending {} to PID {}", signal_name, pid);

    int result = kill(pid, signal);

    if (result == 0) {
        logger_->info("Signal sent successfully");

        // Try to reap zombie
        reap_zombie(pid);

        return {true, ""};
    }

    // kill() failed
    if (errno == ESRCH) {
        // Process doesn't exist (already dead)
        logger_->warn("Process {} already terminated", pid);
        return {true, ""};  // Not an error, desired state achieved
    }

    std::string error = std::string("kill() failed: ") + strerror(errno);
    logger_->error("{}", error);
    return {false, error};
}

void PosixProcessRunner::reap_zombie(int pid) {
    // Non-blocking wait to reap zombie process
    // WNOHANG: Return immediately if no child has exited
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);

    if (result == pid) {
        logger_->debug("Reaped zombie process {}", pid);
    } else if (result == 0) {
        // Process still running, not reaped yet
        logger_->debug("Process {} still running, not reaped", pid);
    } else if (result < 0) {
        // Error (probably ECHILD - not our child)
        logger_->debug("waitpid({}) failed: {}", pid, strerror(errno));
    }
}

} // namespace aknet::jack