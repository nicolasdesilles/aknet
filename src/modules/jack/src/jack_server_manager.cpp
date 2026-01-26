//
// Created by Nicolas Désilles on 25/01/2026.
//

#include "jack_server_manager.h"
#include <stdexcept>
#include <sstream>
#include <thread>
#include <chrono>

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

    Result JackServerManager::wait_for_server_ready() {
        constexpr int max_wait_ms = 5000;
        constexpr int poll_interval_ms = 100;
        int waited_ms = 0;

        logger_->info("Waiting for JACK server to be ready...");

        while (waited_ms < max_wait_ms) {
            auto probe = probe_server();
            if (probe.is_running) {
                logger_->info("JACK server ready after {}ms", waited_ms);
                return {true, ""};
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
            waited_ms += poll_interval_ms;
        }

        return {false, "JACK server failed to become ready within " + std::to_string(max_wait_ms) + "ms timeout"};
    }

    std::vector<std::string> JackServerManager::build_jackd_args(const ServerConfig& config) {
        std::vector<std::string> args = {
            "-R",  // Realtime mode
            "-d", "coreaudio",  // CoreAudio driver
            "-r", std::to_string(config.sample_rate),
            "-p", std::to_string(config.buffer_size)
        };

        // Add device selection if not using system default
        if (config.input_device_id != "system_default") {
            args.push_back("-C");
            args.push_back(config.input_device_id);
            logger_->debug("Input device: {}", config.input_device_id);
        } else {
            logger_->debug("Input device: system default");
        }

        if (config.output_device_id != "system_default") {
            args.push_back("-P");
            args.push_back(config.output_device_id);
            logger_->debug("Output device: {}", config.output_device_id);
        } else {
            logger_->debug("Output device: system default");
        }

        return args;
    }

    Result JackServerManager::ensure_server(const ServerConfig& config, bool force_restart) {
        // Probe current server state
        auto info = probe_server();

        // Case 1: Server not running - start it
        if (!info.is_running) {
            logger_->info("No JACK server running, starting with config: {}Hz, {} samples",
                          config.sample_rate, config.buffer_size);

            // Build jackd arguments for macOS CoreAudio
            std::vector<std::string> args = build_jackd_args(config);

            int pid = 0;
            auto result = process_runner_->spawn(config.executable_path, args, pid);

            if (!result.ok) {
                logger_->error("Failed to start JACK server: {}", result.error);
                return result;
            }

            owned_server_pid_ = pid;
            last_server_config_ = config;
            logger_->info("Started JACK server with PID {}", pid);

            // Wait for server to be ready to accept connections
            auto wait_result = wait_for_server_ready();
            if (!wait_result.ok) {
                logger_->error("JACK server started but not responding: {}", wait_result.error);
                return wait_result;
            }

            return {true, ""};
        }

        // Case 2: Server running with matching config - do nothing
        // If we own the server, check full config including devices
        // If external server, only check sample_rate and buffer_size (can't verify devices)
        bool config_matches = false;

        if (owned_server_pid_.has_value() && last_server_config_.has_value()) {
            // We own this server - compare full config including devices
            const auto& last_config = last_server_config_.value();
            config_matches = (info.sample_rate == config.sample_rate &&
                             info.buffer_size == config.buffer_size &&
                             last_config.input_device_id == config.input_device_id &&
                             last_config.output_device_id == config.output_device_id);
        } else {
            // External server - can only verify sample_rate and buffer_size
            config_matches = (info.sample_rate == config.sample_rate &&
                             info.buffer_size == config.buffer_size);
        }

        if (config_matches) {
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

            // Spawn with new config
            std::vector<std::string> args = build_jackd_args(config);

            int pid = 0;
            auto result = process_runner_->spawn(config.executable_path, args, pid);

            if (!result.ok) {
                logger_->error("Failed to restart JACK server: {}", result.error);
                return result;
            }

            owned_server_pid_ = pid;
            last_server_config_ = config;
            logger_->info("Restarted JACK server with PID {}", pid);

            // Wait for server to be ready to accept connections
            auto wait_result = wait_for_server_ready();
            if (!wait_result.ok) {
                logger_->error("JACK server restarted but not responding: {}", wait_result.error);
                return wait_result;
            }

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
            last_server_config_.reset();
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