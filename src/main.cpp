#include <saucer/smartview.hpp>
#include <saucer/embedded/all.hpp>
#include <core.h>

namespace {
    std::unique_ptr<aknet::core> g_core;
}

coco::stray start(saucer::application* app)
{
    auto window  = saucer::window::create(app).value();
    auto webview = saucer::smartview<>::create({.window = window});

    window->set_title("aknet");

    webview->expose("log_test_msg", []() { g_core->test_function(); });

    webview->embed(saucer::embedded::all());
    webview->serve("/index.html");
    window->show();

    // temp Test function
    g_core->test_function();

    co_await app->finish();
}

int main()
{
    // Initialize aknet core
    g_core = std::make_unique<aknet::core>(aknet::core_config{
        .log_level = aknet::log::LogLevel::trace
    });

    int result = saucer::application::create({.id = "aknet"})->run(start);

    g_core.reset();  // Explicit core shutdown before main() exits

    return result;
}