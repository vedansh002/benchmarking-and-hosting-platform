cmake_minimum_required(VERSION 3.20)

project(load_generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(load_generator
    main.cpp
)

target_include_directories(load_generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (MSVC)
    target_compile_options(load_generator PRIVATE /W4 /permissive-)
else()
    target_compile_options(load_generator PRIVATE -Wall -Wextra -Wpedantic)
endif()