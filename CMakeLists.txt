cmake_minimum_required(VERSION 3.13)
project(bettercppsax VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# add_subdirectory(yaml-cpp)

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE rapidjson/include)

add_library(bettercppsax INTERFACE bettercppsax.h)
target_include_directories(bettercppsax INTERFACE .)
target_link_libraries(bettercppsax INTERFACE 
    #yaml-cpp 
    rapidjson
)

add_executable(read_vviz example/vviz_parser.cpp)
target_link_libraries(read_vviz PUBLIC bettercppsax)

add_subdirectory(tests)
