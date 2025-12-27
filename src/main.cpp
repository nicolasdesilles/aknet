
#include <print>
#include <saucer/smartview.hpp>
#include <saucer/embedded/all.hpp>
#include <core.h>

static aknet::core g_core;

coco::stray start(saucer::application *app)
{
    // Initialize the aknet core before anything else
    g_core.init();

    auto window  = saucer::window::create(app).value();
    auto webview = saucer::smartview<>::create({.window = window});


    window->set_title("aknet");

    // Binding test function
    webview->expose("log_test_msg", aknet::core::test_function);

    webview->embed(saucer::embedded::all());
    webview->serve("/index.html");

    window->show();

    // Test code
    aknet::core::test_function();

    co_await app->finish();

    // Clean shutdown by stopping the aknet core
    g_core.shutdown();
}

int main()
{
    return saucer::application::create({.id = "aknet"})->run(start);
}