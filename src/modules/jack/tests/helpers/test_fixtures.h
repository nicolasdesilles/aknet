//
// Created by Nicolas Désilles on 25/01/2026.
//

#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <memory>
#include <vector>
#include <set>
#include <sstream>
#include <filesystem>
#include <chrono>

#include <logger.h>
#include <settings.h>

#include <jack_interfaces.h>
#include <jack_server_manager.h>

namespace aknet::test {

namespace fs = std::filesystem;

// ------------------------------------------------------------------------------------------------
// Test Helpers
// ------------------------------------------------------------------------------------------------

// Helper to create a temporary test directory
class TempDir {
    fs::path path_;
public:
    TempDir() {
        path_ = fs::temp_directory_path() / ("aknet_jack_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(path_);
    }

    ~TempDir() {
        if (fs::exists(path_)) {
            fs::remove_all(path_);
        }
    }

    fs::path path() const { return path_; }
};

// ------------------------------------------------------------------------------------------------
// Mock Implementations
// ------------------------------------------------------------------------------------------------

/**
 * Mock process runner for tests.
 *
 * Records all spawn/terminate calls for verification.
 */
class MockProcessRunner : public jack::IProcessRunner {
public:
    struct SpawnCall {
        std::string executable;
        std::vector<std::string> args;

        std::string args_string() const {
            std::ostringstream oss;
            for (const auto& arg : args) {
                oss << arg << " ";
            }
            return oss.str();
        }
    };

    struct TerminateCall {
        int pid;
        bool force;
    };

    void set_spawn_result(bool success, int pid = 0) {
        spawn_success_ = success;
        spawn_pid_ = pid;
    }

    jack::Result spawn(
        const std::string& executable,
        const std::vector<std::string>& args,
        int& pid
    ) override {
        spawn_calls_.push_back({executable, args});

        if (spawn_success_) {
            pid = spawn_pid_;
            running_pids_.insert(pid);
            return {true, ""};
        }
        return {false, "Mock spawn failed"};
    }

    bool is_running(int pid) override {
        return running_pids_.count(pid) > 0;
    }

    jack::Result terminate(int pid, bool force) override {
        terminate_calls_.push_back({pid, force});
        running_pids_.erase(pid);
        return {true, ""};
    }

    const std::vector<SpawnCall>& get_spawn_calls() const { return spawn_calls_; }
    const std::vector<TerminateCall>& get_terminate_calls() const { return terminate_calls_; }
    void clear_calls() { spawn_calls_.clear(); terminate_calls_.clear(); }

private:
    bool spawn_success_ = true;
    int spawn_pid_ = 0;
    std::vector<SpawnCall> spawn_calls_;
    std::vector<TerminateCall> terminate_calls_;
    std::set<int> running_pids_;
};

/**
 * Mock JACK client API for tests.
 *
 * Simulates JACK server probe/client operations without real libjack.
 */
class MockJackClientAPI : public jack::IJackClientAPI {
public:
    void set_probe_result(const jack::ServerInfo& info) {
        probe_result_ = info;
    }

    jack::ServerInfo probe_server() override {
        probe_call_count_++;
        return probe_result_;
    }

    jack::Result open_client(const std::string& client_name) override {
        if (client_open_) {
            return {false, "Client already open"};
        }
        client_name_ = client_name;
        client_open_ = true;
        return {true, ""};
    }

    jack::Result register_input_ports(int count) override {
        if (!client_open_) {
            return {false, "Client not open"};
        }
        input_port_count_ = count;
        return {true, ""};
    }

    jack::Result activate() override {
        if (!client_open_) {
            return {false, "Client not open"};
        }
        client_active_ = true;
        return {true, ""};
    }

    jack::Result close_client() override {
        if (!client_open_) {
            return {false, "Client not open"};
        }
        client_open_ = false;
        client_active_ = false;
        return {true, ""};
    }

    bool is_active() const override {
        return client_active_;
    }

    int get_probe_call_count() const { return probe_call_count_; }
    const std::string& get_client_name() const { return client_name_; }
    int get_input_port_count() const { return input_port_count_; }

private:
    jack::ServerInfo probe_result_{0, 0, false};
    int probe_call_count_ = 0;
    bool client_open_ = false;
    bool client_active_ = false;
    std::string client_name_;
    int input_port_count_ = 0;
};

/**
 * Test fixture for JACK server manager tests.
 */
struct JackServerManagerTestFixture {
    std::shared_ptr<log::Logger> logger;
    std::shared_ptr<MockJackClientAPI> client_api;
    std::shared_ptr<MockProcessRunner> process_runner;
    TempDir temp_dir;

    JackServerManagerTestFixture() {
        log::init(temp_dir.path());
        logger = log::get("jack_test");
        client_api = std::make_shared<MockJackClientAPI>();
        process_runner = std::make_shared<MockProcessRunner>();
    }

    ~JackServerManagerTestFixture() {
        log::shutdown();
    }
};


/**
 * Test fixture for JACK client tests.
 */
struct JackClientTestFixture {
    std::shared_ptr<log::Logger> logger;
    std::shared_ptr<MockJackClientAPI> client_api;
    TempDir temp_dir;

    JackClientTestFixture() {
        log::init(temp_dir.path());
        logger = log::get("jack_client_test");
        client_api = std::make_shared<MockJackClientAPI>();
    }

    ~JackClientTestFixture() {
        log::shutdown();
    }
};

} // namespace aknet::test