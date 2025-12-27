
#include <print>
#include <saucer/smartview.hpp>
#include <saucer/embedded/all.hpp>

coco::stray start(saucer::application *app)
{
    auto window  = saucer::window::create(app).value();
    auto webview = saucer::smartview<>::create({.window = window});


    window->set_title("aknet");

    webview->embed(saucer::embedded::all());
    webview->serve("/index.html");

    window->show();


    co_await app->finish();
}

int main()
{
    return saucer::application::create({.id = "aknet"})->run(start);
}