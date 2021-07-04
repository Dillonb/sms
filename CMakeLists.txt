cmake_minimum_required(VERSION 3.20)
enable_testing()

project(sms)

add_compile_options(-Wall -Werror)
add_compile_definitions(LOG_ENABLED)

set(CMAKE_CXX_STANDARD 20)
include_directories(src)

add_subdirectory(src)
add_subdirectory(test)