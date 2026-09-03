cmake_minimum_required(VERSION 3.16)

project(bram_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bram_test
    src/bram_test.cpp
)

# target_link_libraries(bram_test
#     PRIVATE
#     gpiod
#     gpiodcxx
# )