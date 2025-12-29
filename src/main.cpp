
#include <print>
#include <saucer/smartview.hpp>
#include <saucer/embedded/all.hpp>
#include <core.h>

coco::stray start(saucer::application *app)
{
    // Initialize the aknet core before anything else
    aknet::core::init();

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
    aknet::core::shutdown();
}

int main()
{
    return saucer::application::create({.id = "aknet"})->run(start);
}