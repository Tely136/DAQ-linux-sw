cmake_minimum_required(VERSION 3.16)

project(led_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(led_test
    src/led_test.cpp
)

target_link_libraries(led_test
    PRIVATE
    gpiod
    gpiodcxx
)