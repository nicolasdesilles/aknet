//
// Created by Nicolas Désilles on 25/01/2026.
//

#include <jack_module.h>
#include <jack.h>

#include <stdexcept>

namespace aknet::jack {

    JackModule::JackModule(std::shared_ptr<log::Logger> logger)
        : logger_(std::move(logger))
    {
        if (!logger_) {
            throw std::invalid_argument("Logger cannot be null");
        }
    }

    Result JackModule::init(
        const settings::AppSettings& settings,
        std::shared_ptr<IProcessRunner> process_runner,
        std::shared_ptr<IJackClientAPI> client_api,
        std::shared_ptr<IAudioDeviceManager> device_manager)
    {
        if (state_ != JackModuleState::Uninitialized) {
            logger_->error("JackModule already initialized");
            return {false, "Module already initialized"};
        }

        // Validate settings
        if (settings.audio.num_channels < 1) {
            logger_->error("Invalid num_channels: {}", settings.audio.num_channels);
            return {false, "num_channels must be >= 1"};
        }

        if (settings.jack.client_name.empty()) {
            logger_->error("JACK client name cannot be empty");
            return {false, "client_name cannot be empty"};
        }

        logger_->info("Initializing JACK module");
        logger_->info("  Client: {}", settings.jack.client_name);
        logger_->info("  Channels: {}", settings.audio.num_channels);
        logger_->info("  Sample rate: {}Hz", settings.audio.sampling_rate);
        logger_->info("  Buffer size: {} samples", settings.audio.buffer_size);

        settings_ = settings;

        // Create components with factories (or use provided mocks for testing)
        if (!process_runner) {
            process_runner = create_process_runner(logger_);
        }
        if (!client_api) {
            client_api = create_libjack_client_api(logger_);
        }
        if (!device_manager) {
            device_manager = create_device_manager(logger_);
        }

        device_manager_ = device_manager;

        server_manager_ = std::make_unique<JackServerManager>(
            logger_,
            client_api,
            process_runner
        );

        client_ = std::make_unique<JackClient>(
            logger_,
            client_api
        );

        audio_processor_ = std::make_shared<JackAudioProcessor>(
            settings_.audio.num_channels
        );

        state_ = JackModuleState::Initialized;
        logger_->info("JACK module initialized successfully");

        return {true, ""};
    }

    Result JackModule::start() {
        if (state_ == JackModuleState::Uninitialized) {
            logger_->error("Cannot start: module not initialized");
            return {false, "Module not initialized"};
        }

        if (state_ == JackModuleState::Active) {
            logger_->warn("Module already active");
            return {true, ""};  // Already started, not an error
        }

        logger_->info("Starting JACK module");

        // Validate and fallback devices if needed
        settings::AppSettings validated_settings = settings_;
        auto validation_result = validate_and_fallback_devices(validated_settings, true);
        if (!validation_result.ok) {
            logger_->error("Device validation failed: {}", validation_result.error);
            return validation_result;
        }

        // Step 1: Ensure JACK server is running
        ServerConfig server_config{
            .executable_path = settings_.jack.server_executable_path,
            .sample_rate = settings_.audio.sampling_rate,
            .buffer_size = settings_.audio.buffer_size,
            .input_device_id = validated_settings.audio.input_device_id,
            .output_device_id = validated_settings.audio.output_device_id
        };

        auto server_result = server_manager_->ensure_server(
            server_config,
            settings_.jack.auto_manage_server
        );

        if (!server_result.ok) {
            logger_->error("Failed to ensure JACK server: {}", server_result.error);
            return server_result;
        }

        // Step 2: Open JACK client
        auto open_result = client_->open(settings_.jack.client_name);
        if (!open_result.ok) {
            logger_->error("Failed to open JACK client: {}", open_result.error);
            return open_result;
        }

        // Step 3: Register input ports
        auto ports_result = client_->register_input_ports(settings_.audio.num_channels);
        if (!ports_result.ok) {
            logger_->error("Failed to register ports: {}", ports_result.error);
            client_->close();
            return ports_result;
        }

        // Step 4: Set audio processor
        auto processor_result = client_->set_audio_processor(audio_processor_);
        if (!processor_result.ok) {
            logger_->error("Failed to set audio processor: {}", processor_result.error);
            client_->close();
            return processor_result;
        }

        // Step 5: Activate client
        auto activate_result = client_->activate();
        if (!activate_result.ok) {
            logger_->error("Failed to activate client: {}", activate_result.error);
            client_->close();
            return activate_result;
        }

        state_ = JackModuleState::Active;
        logger_->info("JACK module started successfully");

        return {true, ""};
    }

    Result JackModule::stop() {
        if (state_ != JackModuleState::Active) {
            logger_->debug("Module not active, nothing to stop");
            return {true, ""};
        }

        logger_->info("Stopping JACK module");

        auto result = client_->close();
        if (!result.ok) {
            logger_->error("Failed to close client: {}", result.error);
            return result;
        }

        state_ = JackModuleState::Initialized;
        logger_->info("JACK module stopped");

        return {true, ""};
    }

    std::vector<float> JackModule::get_audio_levels() const {
        if (state_ != JackModuleState::Active || !client_) {
            return {};
        }

        auto meters = client_->get_audio_levels();
        std::vector<float> levels;
        levels.reserve(meters.size());

        for (const auto& meter : meters) {
            levels.push_back(meter.rms_db);
        }

        return levels;
    }

    std::vector<float> JackModule::get_peak_levels() const {
        if (state_ != JackModuleState::Active || !client_) {
            return {};
        }

        auto meters = client_->get_audio_levels();
        std::vector<float> peaks;
        peaks.reserve(meters.size());

        for (const auto& meter : meters) {
            peaks.push_back(meter.peak_db);
        }

        return peaks;
    }

    void JackModule::reset_peaks() {
        if (client_) {
            client_->reset_peak_levels();
        }
    }

    bool JackModule::is_ready() const {
        return state_ == JackModuleState::Active;
    }

    JackModuleState JackModule::get_state() const {
        return state_;
    }

    Result JackModule::validate_and_fallback_devices(
        settings::AppSettings& settings,
        bool fallback_to_default)
    {
        if (!device_manager_) {
            logger_->warn("No device manager available, skipping device validation");
            return {true, ""};
        }

        bool input_changed = false;
        bool output_changed = false;

        // Validate input device
        std::string original_input = settings.audio.input_device_id;
        if (original_input != "system_default") {
            auto input_device = device_manager_->get_device_by_id(original_input);

            if (!input_device.has_value()) {
                logger_->warn("Input device '{}' not found", original_input);

                if (fallback_to_default) {
                    logger_->info("Falling back to system_default input device");
                    settings.audio.input_device_id = "system_default";
                    input_changed = true;
                } else {
                    return {false, "Input device '" + original_input + "' not found"};
                }
            } else if (!input_device->can_capture()) {
                logger_->warn("Input device '{}' has no input channels", original_input);

                if (fallback_to_default) {
                    logger_->info("Falling back to system_default input device");
                    settings.audio.input_device_id = "system_default";
                    input_changed = true;
                } else {
                    return {false, "Input device '" + original_input + "' has no input capability"};
                }
            } else {
                logger_->info("Input device '{}' validated ({} channels)",
                            input_device->name, input_device->input_channels);
            }
        } else {
            logger_->info("Using system default input device");
        }

        // Validate output device
        std::string original_output = settings.audio.output_device_id;
        if (original_output != "system_default") {
            auto output_device = device_manager_->get_device_by_id(original_output);

            if (!output_device.has_value()) {
                logger_->warn("Output device '{}' not found", original_output);

                if (fallback_to_default) {
                    logger_->info("Falling back to system_default output device");
                    settings.audio.output_device_id = "system_default";
                    output_changed = true;
                } else {
                    return {false, "Output device '" + original_output + "' not found"};
                }
            } else if (!output_device->can_playback()) {
                logger_->warn("Output device '{}' has no output channels", original_output);

                if (fallback_to_default) {
                    logger_->info("Falling back to system_default output device");
                    settings.audio.output_device_id = "system_default";
                    output_changed = true;
                } else {
                    return {false, "Output device '" + original_output + "' has no output capability"};
                }
            } else {
                logger_->info("Output device '{}' validated ({} channels)",
                            output_device->name, output_device->output_channels);
            }
        } else {
            logger_->info("Using system default output device");
        }

        // Log if fallback occurred
        if (input_changed || output_changed) {
            logger_->warn("Device configuration changed during validation");
            if (input_changed) {
                logger_->warn("  Input: {} -> {}", original_input, settings.audio.input_device_id);
            }
            if (output_changed) {
                logger_->warn("  Output: {} -> {}", original_output, settings.audio.output_device_id);
            }
        }

        return {true, ""};
    }

} // namespace aknet::jack