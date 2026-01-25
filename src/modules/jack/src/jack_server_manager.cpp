//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "jack_server_manager.h"
#include <stdexcept>
#include <sstream>

namespace aknet::jack {

JackServerManager::JackServerManager(
    std::shared_ptr<log::Logger> logger,
    std::shared_ptr<IJackClientAPI> client_api,
    std::shared_ptr<IProcessRunner> process_runner)
    : logger_(std::move(logger))
    , client_api_(std::move(client_api))
    , process_runner_(std::move(process_runner))
{
    if (!logger_) {
        throw std::invalid_argument("Logger cannot be null");
    }
    if (!client_api_) {
        throw std::invalid_argument("Client API cannot be null");
    }
    if (!process_runner_) {
        throw std::invalid_argument("Process runner cannot be null");
    }
}

JackServerManager::~JackServerManager() {
    // Stop owned server on destruction
    stop_server();
}

ServerInfo JackServerManager::probe_server() {
    return client_api_->probe_server();
}

Result JackServerManager::ensure_server(const ServerConfig& config, bool force_restart) {
    // Probe current server state
    auto info = probe_server();

    // Case 1: Server not running - start it
    if (!info.is_running) {
        logger_->info("No JACK server running, starting with config: {}Hz, {} samples",
                      config.sample_rate, config.buffer_size);

        // Build jackd arguments for macOS CoreAudio
        std::vector<std::string> args = {
            "-R",  // Realtime mode
            "-d", "coreaudio",  // CoreAudio driver
            "-r", std::to_string(config.sample_rate),
            "-p", std::to_string(config.buffer_size)
        };

        int pid = 0;
        auto result = process_runner_->spawn(config.executable_path, args, pid);

        if (!result.ok) {
            logger_->error("Failed to start JACK server: {}", result.error);
            return result;
        }

        owned_server_pid_ = pid;
        logger_->info("Started JACK server with PID {}", pid);
        return {true, ""};
    }

    // Case 2: Server running with matching config - do nothing
    if (info.sample_rate == config.sample_rate && info.buffer_size == config.buffer_size) {
        logger_->info("JACK server already running with correct settings ({}Hz, {} samples)",
                      info.sample_rate, info.buffer_size);
        return {true, ""};
    }

    // Case 3: Server running with wrong config
    logger_->warn("JACK server running with mismatched settings: {}Hz, {} samples (expected {}Hz, {} samples)",
                  info.sample_rate, info.buffer_size, config.sample_rate, config.buffer_size);

    // Check if we own this server
    if (owned_server_pid_.has_value()) {
        // We own it - restart with new settings
        logger_->info("Restarting owned JACK server to apply new settings");

        auto stop_result = stop_server();
        if (!stop_result.ok) {
            return stop_result;
        }

        // Note: In production, might need to wait/poll for server to fully stop
        // For tests with mocks, this is immediate

        // Spawn with new config
        std::vector<std::string> args = {
            "-R",
            "-d", "coreaudio",
            "-r", std::to_string(config.sample_rate),
            "-p", std::to_string(config.buffer_size)
        };

        int pid = 0;
        auto result = process_runner_->spawn(config.executable_path, args, pid);

        if (!result.ok) {
            logger_->error("Failed to restart JACK server: {}", result.error);
            return result;
        }

        owned_server_pid_ = pid;
        logger_->info("Restarted JACK server with PID {}", pid);
        return {true, ""};
    }

    // External server with wrong settings
    // Policy: Fail gracefully rather than killing user's server
    std::ostringstream oss;
    oss << "External JACK server has mismatched settings: "
            << info.sample_rate << "Hz, " << info.buffer_size << " samples "
            << "(expected " << config.sample_rate << "Hz, " << config.buffer_size << " samples). "
            << "Cannot automatically restart external server.";

    logger_->error("{}", oss.str());
    return {false, oss.str()};
}

Result JackServerManager::stop_server() {
    if (!owned_server_pid_.has_value()) {
        logger_->debug("No owned server to stop");
        return {true, ""};
    }

    int pid = owned_server_pid_.value();
    logger_->info("Stopping JACK server with PID {}", pid);

    auto result = process_runner_->terminate(pid, false);

    if (result.ok) {
        owned_server_pid_.reset();
        logger_->info("JACK server stopped");
    } else {
        logger_->error("Failed to stop JACK server: {}", result.error);
    }

    return result;
}

bool JackServerManager::owns_server() const {
    return owned_server_pid_.has_value();
}

} // namespace aknet::jack