
add_subdirectory(simdjson)
add_subdirectory(json-3.11.3)

add_executable(benchmarks benchmarks.cpp vviz_data.h)
target_link_libraries(benchmarks PRIVATE simdjson nlohmann_json::nlohmann_json rapidjson)


add_executable(benchmarks_track_memory benchmarks.cpp vviz_data.h)
target_compile_definitions(benchmarks_track_memory PUBLIC TRACK_MEMORY)
target_link_libraries(benchmarks_track_memory PRIVATE simdjson nlohmann_json::nlohmann_json rapidjson)
