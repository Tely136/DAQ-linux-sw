#include <gpiod.hpp>
#include <chrono>
#include <thread>
#include <iostream>

int main()
{
    auto chip = gpiod::chip("/dev/gpiochip0");

    auto settings = gpiod::line_settings()
        .set_direction(gpiod::line::direction::OUTPUT);

    auto config = gpiod::line_config();
    config.add_line_settings(0, settings);

    auto request = chip.prepare_request()
        .set_consumer("cpp-blink")
        .set_line_config(config)
        .do_request();

    std::cout << "Blinking LED on pin 0...\n";

    while (true) {
        request.set_value(0, gpiod::line::value::ACTIVE);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        request.set_value(0, gpiod::line::value::INACTIVE);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}