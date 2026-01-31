//
// Created by Nicolas Désilles on 27/12/2025.
//

#include "core.h"
#include <version.h>
#include <bridge.h>
#include <jack.h>
#include <saucer/smartview.hpp>
#include <steps_definition.h>
#include <optional>
#include <chrono>
#include <thread>
#include <jack/jack.h>



namespace aknet {

    // Constructor
    core::core(const core_config& config) {

        // Initialize logging infrastructure (the core owns it)
        log::init(config.log_dir);
        log::set_global_log_level(config.log_level);

        // Get a logger for the core
        logger_ = log::get("core");

        log_aknet_start_message();
        logger_->info("Initializing Core...");

        // Start the settings system
        auto settings_config = settings::SettingsConfig{
        .base_dir = config.settings_dir,
        .schema_version = config.settings_schema_version};
        
        auto settings_logger = log::get("settings");
        
        settings_.init(settings_logger, settings_config);
        
        // Loading saved settings
        settings_.load_or_create();

        // Restart rules for settings changes
        settings_.add_restart_rule({
            .key = "audio.num_channels",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });
        settings_.add_restart_rule({
            .key = "audio.sampling_rate",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });
        settings_.add_restart_rule({
            .key = "audio.buffer_size",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });
        settings_.add_restart_rule({
            .key = "jack.client_name",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });
        settings_.add_restart_rule({
            .key = "audio.input_device_id",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });
        settings_.add_restart_rule({
            .key = "audio.output_device_id",
            .requires_app_restart = false,
            .module_name_to_restart = "jack"
        });

        // Applying log level stored in settings
        log::set_global_log_level(log::string_to_log_level(settings_.snapshot()->general.log_level));

        // Startup manager init
        auto startup_logger = log::get("startup");

        // Create a shared_ptr with a no-op deleter since core owns settings_
        auto settings_ptr = std::shared_ptr<settings::Settings>(
            &settings_,
            [](settings::Settings*){} // no-op deleter
        );

        // Initialize Audio module
        audio_module_ = audio::create_audio_module(logger_);
        logger_->info("Audio module created");

        // Initialize JACK module
        jack_module_ = std::make_shared<jack::JackModule>(logger_);
        logger_->info("JACK module created");


        // Initialize StartupManager
        startup_manager_ = std::make_unique<startup::StartupManager>(
            startup_logger,
            settings_ptr
        );

        // Pass jack_module to startup manager
        startup_manager_->set_jack_module(jack_module_);

        // Register startup steps
        startup_manager_->set_steps(startup::create_default_steps());

        logger_->info("Initializing Core: Done.");
    }

    // Destructor
    core::~core() {
        logger_->info("Core shutting down...");

        stop_status_tick();

        // Destroy modules
        if (bridge_) {
            bridge_->disconnect();
        }
        bridge_.reset();
        jack_module_.reset();
        audio_module_.reset();

        startup_manager_.reset();
        
        // Shutdown settings system
        settings_.shutdown();

        // Release our logger before shutting down logging system
        logger_.reset();

        // Shutdown logging infrastructure last
        log::shutdown();
    }

    startup::StepContext core::create_step_context() {
        startup::StepContext ctx;
        ctx.logger = logger_;
        ctx.settings = &settings_;
        ctx.jack_module = jack_module_;
        return ctx;
    }

    void core::test_function() {
        logger_->info("Core test function called");
    }

    void core::start_status_tick() {
        if (status_tick_thread_.joinable()) {
            return;
        }

        status_tick_stop_.store(false, std::memory_order_release);

        status_tick_thread_ = std::thread([this]() {
            jack_client_t* cpu_client = nullptr;

            auto close_cpu_client = [&]() {
                if (cpu_client) {
                    jack_client_close(cpu_client);
                    cpu_client = nullptr;
                }
            };

            while (!status_tick_stop_.load(std::memory_order_acquire)) {
                const bool is_active = startup_manager_ &&
                    startup_manager_->get_state() == startup::AppState::Active;

                if (!is_active) {
                    close_cpu_client();
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }

                std::optional<double> cpu_load;

                if (!cpu_client) {
                    jack_status_t status;
                    cpu_client = jack_client_open(
                        "aknet_status_probe",
                        JackNoStartServer,
                        &status
                    );
                }

                if (cpu_client) {
                    cpu_load = jack_cpu_load(cpu_client);
                } else {
                    cpu_load.reset();
                }

                if (bridge_) {
                    try {
                        auto snapshot_str = get_status_snapshot_json();
                        nlohmann::json payload = nlohmann::json::parse(snapshot_str);
                        payload["jackRuntime"]["cpuLoad"] =
                            cpu_load.has_value() ? nlohmann::json(*cpu_load) : nlohmann::json(nullptr);
                        bridge_->dispatch_event("status:tick", payload);
                    } catch (const std::exception& e) {
                        logger_->warn("Failed to dispatch status tick: {}", e.what());
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            close_cpu_client();
        });
    }

    void core::stop_status_tick() {
        status_tick_stop_.store(true, std::memory_order_release);
        if (status_tick_thread_.joinable()) {
            status_tick_thread_.join();
        }
    }

    template<typename WebviewT>
    void core::init_bridge(WebviewT* webview) {
        if (bridge_) {
            logger_->warn("Bridge already initialized");
            return;
        }

        auto bridge_logger = log::get("bridge");
        bridge_ = std::make_unique<bridge::EventBridge>(bridge_logger, webview);

        auto manager_ptr = std::shared_ptr<startup::StartupManager>(
            startup_manager_.get(),
            [](startup::StartupManager*){}
        );

        bridge_->connect_startup_events(manager_ptr);

        start_status_tick();

        logger_->info("Bridge initialized and connected to startup events");
    }

    // Explicit template instantiation for saucer::smartview
    template void core::init_bridge(saucer::smartview<>*);

    bool core::start_startup() {
        logger_->info("Starting startup sequence...");
        auto result = startup_manager_->start_async();
        if (!result.ok) {
            logger_->error("Failed to start startup sequence: {}", result.error);
            return false;
        }
        return true;
    }

    void core::abort_startup() {
        logger_->info("Aborting startup sequence...");
        startup_manager_->request_abort(startup::AbortReason::UserRequested);
    }

    bool core::retry_startup() {
        logger_->info("Retrying startup sequence...");
        auto result = startup_manager_->retry_async();
        if (!result.ok) {
            logger_->error("Failed to retry startup sequence: {}", result.error);
            return false;
        }
        return true;
    }

    void core::set_test_mode(int mode) {
        logger_->warn("Test mode {} requested - fake steps removed, using production steps", mode);
        startup_manager_->clear_steps();
        startup_manager_->set_steps(startup::create_default_steps());
    }

    std::string core::get_settings_json() {
        auto snapshot = settings_.snapshot();
        return settings::to_json_string(*snapshot);
    }

    std::string core::get_pending_settings_json() {
        auto pending = settings_.pending_copy();
        return settings::to_json_string(pending);
    }

    std::string core::stage_settings_json(const std::string& json_str) {
        settings::AppSettings new_settings;
        auto parse_result = settings::from_json_string(json_str, new_settings);

        if (!parse_result.ok) {
            nlohmann::json error_json = {
                {"ok", false},
                {"error", "Failed to parse settings JSON: " + parse_result.error}
            };
            return error_json.dump();
        }

        auto stage_result = settings_.stage([&new_settings](settings::AppSettings& s) {
            s = new_settings;
        });

        nlohmann::json result_json = {
            {"ok", stage_result.ok},
            {"error", stage_result.error}
        };
        return result_json.dump();
    }

    std::string core::save_settings_json() {
        auto [result, impact] = settings_.save();

        nlohmann::json result_json = {
            {"ok", result.ok},
            {"error", result.error}
        };

        nlohmann::json impact_json = {
            {"app_restart_required", impact.app_restart_required},
            {"modules_restart_required", impact.modules_restart_required},
            {"restart_sensitive_keys_changed", impact.restart_sensitive_keys_changed}
        };

        nlohmann::json response = {
            {"result", result_json},
            {"save_impact", impact_json}
        };

        return response.dump();
    }

    std::string core::reset_pending_settings_json() {
        auto result = settings_.reset_pending_to_active();

        nlohmann::json result_json = {
            {"ok", result.ok},
            {"error", result.error}
        };
        return result_json.dump();
    }

    bool core::has_pending_settings_changes() {
        return settings_.has_pending_changes();
    }

    std::string core::get_audio_devices_json() {
        if (!jack_module_) {
            nlohmann::json error = {
                {"error", "Jack module not initialized"},
                {"devices", nlohmann::json::array()}
            };
            return error.dump();
        }

        try {
            // Get device manager from jack module
            // We need to create a device manager since it's not exposed
            auto logger = log::get("core");
            auto device_manager = jack::create_device_manager(logger);

            auto devices = device_manager->enumerate_devices();

            nlohmann::json devices_json = nlohmann::json::array();

            for (const auto& device : devices) {
                nlohmann::json device_json = {
                    {"id", device.id},
                    {"name", device.name},
                    {"input_channels", device.input_channels},
                    {"output_channels", device.output_channels},
                    {"is_default", device.is_default}
                };
                devices_json.push_back(device_json);
            }

            return devices_json.dump();

        } catch (const std::exception& e) {
            nlohmann::json error = {
                {"error", std::string("Failed to enumerate devices: ") + e.what()},
                {"devices", nlohmann::json::array()}
            };
            return error.dump();
        }
    }

    std::string core::get_default_audio_device_json() {
        if (!jack_module_) {
            nlohmann::json error = {
                {"error", "Jack module not initialized"}
            };
            return error.dump();
        }

        try {
            auto logger = log::get("core");
            auto device_manager = jack::create_device_manager(logger);

            auto device = device_manager->get_default_device();

            nlohmann::json device_json = {
                {"id", device.id},
                {"name", device.name},
                {"input_channels", device.input_channels},
                {"output_channels", device.output_channels},
                {"is_default", device.is_default}
            };

            return device_json.dump();

        } catch (const std::exception& e) {
            nlohmann::json error = {
                {"error", std::string("Failed to get default device: ") + e.what()}
            };
            return error.dump();
        }
    }

    std::string core::get_status_snapshot_json() {
        auto snapshot = settings_.snapshot();

        auto app_state = startup_manager_ ? startup_manager_->get_state() : startup::AppState::Off;
        std::string app_status;
        switch (app_state) {
            case startup::AppState::Off:
                app_status = "off";
                break;
            case startup::AppState::Booting:
                app_status = "booting";
                break;
            case startup::AppState::Active:
                app_status = "active";
                break;
            case startup::AppState::ShuttingDown:
                app_status = "shutting_down";
                break;
            default:
                app_status = "error";
                break;
        }

        bool server_running = false;

        try {
            auto client_api = jack::create_libjack_client_api(logger_);
            auto server_info = client_api->probe_server();
            server_running = server_info.is_running;
        } catch (const std::exception& e) {
            logger_->warn("Failed to probe JACK server: {}", e.what());
        }

        bool client_connected = false;
        int channel_count = snapshot->audio.num_channels;
        if (jack_module_) {
            client_connected = jack_module_->is_client_active();
            int port_count = jack_module_->get_client_input_port_count();
            if (port_count > 0) {
                channel_count = port_count;
            }
        }

        nlohmann::json response = {
            {"app", {{"status", app_status}}},
            {"jackServer", {{"running", server_running}}},
            {"jackClient", {{"connected", client_connected}, {"channels", channel_count}}},
            {"audio", {{"sampleRate", snapshot->audio.sampling_rate}, {"bufferSize", snapshot->audio.buffer_size}}},
            {"jackRuntime", {{"cpuLoad", nullptr}}}
        };

        return response.dump();
    }

    void core::log_aknet_start_message() {
        if (logger_) {
            logger_->info("--------------------------------------------------------");
            logger_->info("      ▄▄                  ██  ");
            logger_->info(" ▀▀█▄ ██ ▄█▀ ████▄ ▄█▀█▄ ▀██▀▀");
            logger_->info("▄█▀██ ████   ██ ██ ██▄█▀  ██  ");
            logger_->info("▀█▄██ ██ ▀█▄ ██ ██ ▀█▄▄▄  ██  ");
            logger_->info("");
            logger_->info("aknet - v{} by {}", PROJECT_VERSION, PROJECT_AUTHOR);
            logger_->info("--------------------------------------------------------");
        }
    }
} // namespace aknet
