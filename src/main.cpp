#include <saucer/smartview.hpp>
#include <saucer/embedded/all.hpp>
#include <core.h>

namespace {
    std::unique_ptr<aknet::core> g_core;
}

coco::stray start(saucer::application* app)
{
    auto window  = saucer::window::create(app).value();
    auto webview = saucer::smartview<>::create({.window = window}).value();

    window->set_title("aknet");

    // Initialize the UI bridge
    g_core->init_bridge(&webview);

    webview.expose("log_test_msg", []() { g_core->test_function(); });

    // Startup control functions
    webview.expose("start_startup", []() -> bool { 
        return g_core->start_startup(); 
    });

    webview.expose("abort_startup", []() { 
        g_core->abort_startup(); 
    });

    webview.expose("retry_startup", []() -> bool { 
        return g_core->retry_startup(); 
    });

    webview.expose("set_test_mode", [](int mode) { 
        g_core->set_test_mode(mode); 
    });

    // Settings control functions
    webview.expose("get_settings", []() -> std::string {
        return g_core->get_settings_json();
    });

    webview.expose("get_pending_settings", []() -> std::string {
        return g_core->get_pending_settings_json();
    });

    webview.expose("stage_settings", [](const std::string& json_str) -> std::string {
        return g_core->stage_settings_json(json_str);
    });

    webview.expose("save_settings", []() -> std::string {
        return g_core->save_settings_json();
    });

    webview.expose("reset_pending_settings", []() -> std::string {
        return g_core->reset_pending_settings_json();
    });

    webview.expose("has_pending_settings_changes", []() -> bool {
        return g_core->has_pending_settings_changes();
    });

    webview.expose("get_audio_devices", []() -> std::string {
        return g_core->get_audio_devices_json();
    });

    webview.expose("get_default_audio_device", []() -> std::string {
        return g_core->get_default_audio_device_json();
    });

    webview.embed(saucer::embedded::all());
    webview.serve("/index.html");
    window->show();

    // temp Test function
    g_core->test_function();

    co_await app->finish();
}

int main()
{
    // Initialize aknet core
    const char* home = std::getenv("HOME");
    g_core = std::make_unique<aknet::core>(aknet::core_config{
        .settings_dir = std::filesystem::path(home) / "Desktop" / "Settings" ,
        .settings_schema_version = 1,
        .log_level = aknet::log::LogLevel::trace
    });

    int result = saucer::application::create({.id = "aknet"})->run(start);

    g_core.reset();  // Explicit core shutdown before main() exits

    return result;
}