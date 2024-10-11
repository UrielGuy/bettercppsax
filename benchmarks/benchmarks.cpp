
#include <print>
#include <filesystem>
#include <fstream>
#include <vector>
#include <stacktrace>
#include <unordered_map>
#include <iostream>
#include "vviz_data.h"

namespace fs { using namespace std::filesystem; }

struct memory_data {
    int64_t allocations_count = 0;
    int64_t live_allocations = 0;
    int64_t max_live_allocations = 0;
    int64_t memory_allocated = 0;
    int64_t memory_used = 0;
    int64_t max_memory_used = 0;
} current_memory;

// Macros to get the size of a pointer (Platform dependent).
#if defined(__linux__) || defined(__unix__) || defined(__unix) // Linux/Unix
#include <malloc.h>
#define getPointerSize(ptr) malloc_usable_size(ptr)
#elif __APPLE__ // MacOS
#include <malloc/malloc.h>
#define getPointerSize(ptr) malloc_size(ptr)
#elif defined(_WIN32) || defined(_MSC_VER) // Windows
#include <malloc.h>
#define getPointerSize(ptr) _msize(ptr)
#else
#error "OS not supported!"
#endif

std::unordered_map<std::stacktrace, uint64_t> allocations; 
bool follow_allocations = false;

#ifdef TRACK_MEMORY
void* operator new(size_t size)
{
    thread_local bool in_new = false;

    current_memory.allocations_count++;
    current_memory.live_allocations++;
    current_memory.max_live_allocations = std::max(current_memory.max_live_allocations, current_memory.live_allocations);
    current_memory.memory_allocated += size;
    current_memory.memory_used += size;
    current_memory.max_memory_used = std::max(current_memory.max_memory_used, current_memory.memory_used);

    if (!in_new) {
        bool own_flag = !in_new;
        in_new = true;
        if (follow_allocations && current_memory.allocations_count % 1000 == 0) {
            std::println(std::cout, "{}", current_memory.allocations_count);
            std::cout.flush();
        }
        if (own_flag && follow_allocations)  allocations[std::stacktrace::current()]++;

        if (own_flag) in_new = false;
    }
    void* p = malloc(size);
    return p;
}

void operator delete(void* p)
{
    if (!p) return;
    current_memory.live_allocations--;
    current_memory.memory_used -= getPointerSize(p);
    free(p);
}

#endif

void reset_memory_tracking() {
    current_memory = memory_data();
    allocations.clear();
}

using byte = std::byte;

template<typename T>
struct fake_vector {
    T& emplace_back() {
        return item;
    }

    void push_back(T& item) {
        return;
    }

    T item;


    using value_type = T;
};

std::vector<byte> load_file(fs::path input) {
    std::ifstream file(input, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<byte> buffer(size);
    file.read((char*)buffer.data(), size);

    return buffer;
}


#include <nlohmann/json.hpp>

nlohmann::json nlohmann_load_file(fs::path input_path) {
    using namespace nlohmann;
    std::ifstream f(input_path);
    json res;
    res = res.parse(f);
    return std::move(res);
}

nlohmann::json nlohmann_load_memory(std::vector<byte> data) {
    using namespace nlohmann;
    json res;
    res = res.parse(data.begin(), data.end());
    return std::move(res);
}

template<template<typename> typename T>
void nlohmann_parse(const nlohmann::json& root, show_data<T>& target) {
    target.version = root["version"].get<std::string>();
    target.defaultColorRate = root["defaultColorRate"].get<double>();
    target.defaultPositionRate = root["defaultPositionRate"].get<double>();
    target.timeOffsetSecs = root["timeOffsetSecs"].get<double>();
    for (const auto& perf_data : root["performances"]) {
        auto& performance = target.performances.emplace_back();
        performance.id = perf_data["id"].get<int>();
        
        performance.start_pos.x = perf_data["agentDescription"]["homeX"].get<double>();
        performance.start_pos.y = perf_data["agentDescription"]["homeY"].get<double>();
        performance.start_pos.z = perf_data["agentDescription"]["homeZ"].get<double>();

        for (const auto& location_data : perf_data["agentDescription"]["agentTraversal"]) {
            auto& traversal = performance.agent_traversal.emplace_back();

            traversal.location_delta.x = location_data["dx"].get<double>();
            traversal.location_delta.y = location_data["dy"].get<double>();
            traversal.location_delta.z = location_data["dz"].get<double>();

            if (location_data.contains("dt")) traversal.delay_seconds = location_data["dt"].get<double>();
        }

        for (const auto& payload_data : perf_data["payloadDescription"]) {
            auto& payload = performance.payload_actions.emplace_back();
            payload.id = payload_data["id"].get<int>();
            payload.type = payload_data["type"].get<std::string>();

            for (const auto& action_data : payload_data["payloadActions"]) {
                auto& action = payload.payload_actions.emplace_back();

                action.color.r = action_data["r"].get<uint8_t>();
                action.color.g = action_data["g"].get<uint8_t>();
                action.color.b = action_data["b"].get<uint8_t>();

                if (action_data.contains("frames")) action.frames = action_data["frames"].get<int>();
            }
        }
    }
}

#include <rapidjson/rapidjson.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/istreamwrapper.h"

rapidjson::Document rapidjson_load_file(fs::path input_path) {
    using namespace rapidjson;
    
    std::ifstream f(input_path);
    Document res;
    IStreamWrapper wrapper(f);
    res.ParseStream(wrapper);
    return std::move(res);
}

rapidjson::Document rapidjson_load_memory(std::vector<byte> data) {
    using namespace rapidjson;
    Document res;
    res.Parse((char*)data.data(), data.size());
    return std::move(res);
}

template<template<typename> typename T>
void rapidjson_parse(const rapidjson::Document& root, show_data<T>& target) {
    target.version = root["version"].GetString();
    target.defaultColorRate = root["defaultColorRate"].GetDouble();
    target.defaultPositionRate = root["defaultPositionRate"].GetDouble();
    target.timeOffsetSecs = root["timeOffsetSecs"].GetDouble();
    for (const auto& perf_data : root["performances"].GetArray()) {
        auto& performance = target.performances.emplace_back();
        performance.id = perf_data["id"].GetInt();
        performance.start_pos.x = perf_data["agentDescription"]["homeX"].GetDouble();
        performance.start_pos.y = perf_data["agentDescription"]["homeY"].GetDouble();
        performance.start_pos.z = perf_data["agentDescription"]["homeZ"].GetDouble();
        for (const auto& location_data : perf_data["agentDescription"].GetObject()["agentTraversal"].GetArray()) {
            auto& traversal = performance.agent_traversal.emplace_back();
            traversal.location_delta.x = location_data["dx"].GetDouble();
            traversal.location_delta.y = location_data["dy"].GetDouble();
            traversal.location_delta.z = location_data["dz"].GetDouble();
            if (location_data.HasMember("dt")) traversal.delay_seconds = location_data["dt"].GetDouble();
        }
        for (const auto& payload_data : perf_data["payloadDescription"].GetArray()) {
            auto& payload = performance.payload_actions.emplace_back();
            payload.id = payload_data["id"].GetInt();
            payload.type = payload_data["type"].GetString();
            for (const auto& action_data : payload_data["payloadActions"].GetArray()) {
                auto& action = payload.payload_actions.emplace_back();
                action.color.r = action_data["r"].GetUint();
                action.color.g = action_data["g"].GetUint();
                action.color.b = action_data["b"].GetUint();
                if (action_data.HasMember("frames")) action.frames = action_data["frames"].GetInt();
            }
        }
    }
}

#include "simdjson/simdjson.h"

simdjson::padded_string simdjson_load_file(fs::path input_path) {
    using namespace simdjson;
    return std::move(padded_string::load(input_path.string()).value());
}

simdjson::padded_string simdjson_load_memory(std::vector<byte> data) {
    using namespace simdjson;
    return std::move(padded_string((char*)data.data(), data.size()));
}

template<template<typename> typename T>
void simdjson_parse(const simdjson::padded_string& root, show_data<T>& target) {
    using namespace simdjson;

    ondemand::parser parser;
    auto doc = parser.iterate(root);
    
    auto root_obj = doc.get_object();

    for (auto field : root_obj) {
        std::string_view key = field.unescaped_key();
        if (key == "version") target.version = (std::string_view)field.value().get_string();
        else if (key == "defaultPositionRate") target.defaultPositionRate = field.value().get_double();
        else if (key == "defaultColorRate") target.defaultColorRate = field.value().get_double();
        else if (key == "timeOffsetSecs") target.timeOffsetSecs = field.value().get_double();
        else if (key == "performances") {
            for (auto performance : field.value().get_array()) {
                auto& perf = target.performances.emplace_back();
                for (auto performance_key : performance.get_object()) {
                    std::string_view key = performance_key.unescaped_key();
                    if (key == "id") perf.id = (int)performance_key.value().get_int64();
                    else if (key == "agentDescription") {
                        for (auto field : performance_key.value().get_object()) {
                            std::string_view key = field.unescaped_key();
                            if (key == "homeX") perf.start_pos.x = field.value().get_double();
                            else if (key == "homeY") perf.start_pos.y = field.value().get_double();
                            else if (key == "homeZ") perf.start_pos.z = field.value().get_double();
                            else if (key == "agentTraversal") {
                                for (auto item : field.value().get_array()) {
                                    auto& traversal = perf.agent_traversal.emplace_back();
                                    for (auto field : item.get_object()) {
                                        std::string_view key = field.unescaped_key();
                                        if (key == "dx")      traversal.location_delta.x = field.value().get_double();
                                        else if (key == "dy") traversal.location_delta.y = field.value().get_double();
                                        else if (key == "dz") traversal.location_delta.z = field.value().get_double();
                                        else if (key == "dt") traversal.delay_seconds = field.value().get_double();
                                    }
                                }
                            }
                        }
                    }
                    else if (key == "payloadDescription") {
                        for (auto item : performance_key.value().get_array()) {
                            auto& payload = perf.payload_actions.emplace_back();
                            for (auto field : item.get_object()) {
                                std::string_view key = field.unescaped_key();
                                if (key == "id") payload.id = (int)field.value().get_int64();
                                else if (key == "type") payload.type = field.value().get_string().value();
                                else if (key == "payloadActions") {
                                    for (auto item : field.value().get_array()) {
                                        auto action = payload.payload_actions.emplace_back();
                                        for (auto field : item.get_object()) {
                                            std::string_view key = field.unescaped_key();
                                            if (key == "r") action.color.r =      (uint8_t)field.value().get_int64();
                                            else if (key == "g") action.color.g = (uint8_t)field.value().get_int64();
                                            else if (key == "b") action.color.b = (uint8_t)field.value().get_int64();
                                            else if (key == "frames") action.frames = (int)field.value().get_int64();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


#include "../bettercppsax.h"

using namespace bettercppsax;

template<template<typename> typename T>
class VVIZParser {
private:
    static auto ParseTraversal(std::string_view key, drone_location_data& traversal) {
        if (key == "dx") return ParseScalar(traversal.location_delta.x);
        else if (key == "dy") return ParseScalar(traversal.location_delta.y);
        else if (key == "dz") return ParseScalar(traversal.location_delta.z);
        else if (key == "dt") { traversal.delay_seconds = 1.0; return ParseScalar(traversal.delay_seconds.value()); }
        else return ParseError("Unexpected key in traversal list");
    }

    static auto ParseAgentData(std::string_view key, drone_data<T>& drone) {
        if (key == "homeX") return ParseScalar(drone.start_pos.x);
        else if (key == "homeY") return ParseScalar(drone.start_pos.y);
        else if (key == "homeZ") return ParseScalar(drone.start_pos.z);
        else if (key == "agentTraversal") return ParseList(drone.agent_traversal, ParseTraversal);
        else return SkipNextElement();
    }

    static auto ParseDroneAction(std::string_view key, drone_action& action) {
        if (key == "r") return ParseScalar(action.color.r);
        else if (key == "g") return ParseScalar(action.color.g);
        else if (key == "b") return ParseScalar(action.color.b);
        else if (key == "frames") { action.frames = 1; return ParseScalar(action.frames.value()); }
        else return ParseError("Unexpected key in action ");
    }

    static auto ParseDronePayload(std::string_view key, drone_payload<T>& payload) {
        if (key == "id") return ParseScalar(payload.id);
        else if (key == "type") return ParseScalar(payload.type);
        else if (key == "payloadActions") return ParseList(payload.payload_actions, ParseDroneAction);
        else return SkipNextElement();
    }

    static auto ParsePerformance(std::string_view key, drone_data<T>& drone) {
        if (key == "id") return ParseScalar(drone.id);
        else if (key == "agentDescription") return ParseObject<drone_data<T>>(drone, ParseAgentData);
        else if (key == "payloadDescription") return ParseList(drone.payload_actions, ParseDronePayload);
        else return SkipNextElement();
    }
public:
    static auto ParseRoot(std::string_view key, show_data<T>& data) {
        if (key == "version") return ParseScalar(data.version);
        else if (key == "defaultPositionRate") return ParseScalar(data.defaultPositionRate);
        else if (key == "defaultColorRate") return ParseScalar(data.defaultColorRate);
        else if (key == "timeOffsetSecs") return ParseScalar(data.timeOffsetSecs);
        else if (key == "performances") return ParseList(data.performances, ParsePerformance);
        else return SkipNextElement();
    }
};

std::unique_ptr<std::istream> bettercppsax_load_file(fs::path input) {
    return std::move(std::make_unique<std::ifstream>(input));
}

std::unique_ptr<std::istream> bettercppsax_load_memory(std::vector<byte> data) {
    std::string s{ (char*)data.data(), data.size() };
        
    return std::move(std::make_unique<std::stringstream>(s));
}

template<template<typename> typename T>
int bettercppsax_parse(std::istream& data, show_data<T>& show) 
{
    auto res = bettercppsax::ParseJson<show_data<T>>(data, show, VVIZParser<T>::ParseRoot);

    return 1;
}


int main(int argc, char** argv) {

    if (argc != 5) {
        std::print("Usage: benchmarks <bettercppsax|rapidjson|nlohmann|simdjson> <memory/file> <vector/fake> filename.json");
        return 1;
    }

    show_data<std::vector> data;
    show_data<fake_vector> no_alloc_data;

    std::string_view type = argv[1];
    std::string_view mode = argv[2];
    std::string_view vec_mode = argv[3];
    fs::path input = argv[4];

    bool use_file;
    if (mode == "memory") use_file = false;
    else if (mode == "file") use_file = true;
    else {
        std::print("Usage: benchmarks <bettercppsax|rapidjson|nlohmann|simdjson> <memory/file> <vector/fake> filename.json");
        return 1;
    }

    bool real_vector; 
    if (vec_mode == "fake") real_vector = false;
    else if (vec_mode == "vector") real_vector = true;
    else {
        std::print("Usage: benchmarks <bettercppsax|rapidjson|nlohmann|simdjson> <memory/file> <vector/fake> filename.json");
        return 1;
    }

    std::vector<byte> preloaded;
    if (mode == "memory") preloaded = load_file(input);
    auto start_time = std::chrono::high_resolution_clock::now();
    decltype(start_time) load_ended_time;
//    std::println("Loading data");
    
//    follow_allocations = true;
    
    reset_memory_tracking();
    memory_data load_memory;
    memory_data parse_memory;
    if (type == "rapidjson") {
        auto loaded = use_file ? rapidjson_load_file(input) : rapidjson_load_memory(preloaded);
        load_ended_time = std::chrono::high_resolution_clock::now();
        load_memory = current_memory;
//        std::println("Parsing data");
        reset_memory_tracking();
        if (real_vector) rapidjson_parse(loaded, data);
        else             rapidjson_parse(loaded, no_alloc_data);

        parse_memory = current_memory;
    }
    else if (type == "nlohmann") {
        auto loaded = use_file ? nlohmann_load_file(input) : nlohmann_load_memory(preloaded);
        load_ended_time = std::chrono::high_resolution_clock::now();
        load_memory = current_memory;
//        std::println("Parsing data");
        reset_memory_tracking();
        if (real_vector) nlohmann_parse(loaded, data );
        else             nlohmann_parse(loaded, no_alloc_data);

        parse_memory = current_memory;
    }
    else if (type == "simdjson") {
        auto loaded = use_file ? simdjson_load_file(input) : simdjson_load_memory(preloaded);
        load_ended_time = std::chrono::high_resolution_clock::now();
        load_memory = current_memory;
//        std::println("Parsing data");
        reset_memory_tracking();
        if (real_vector) simdjson_parse(loaded, data);
        else             simdjson_parse(loaded, no_alloc_data);

        parse_memory = current_memory;
    }
    else if (type == "bettercppsax") {
        auto loaded = use_file ? bettercppsax_load_file(input) : bettercppsax_load_memory(preloaded);
        load_ended_time = std::chrono::high_resolution_clock::now();
        load_memory = current_memory;
//        std::println("Parsing data");
        reset_memory_tracking();
        if (real_vector) bettercppsax_parse(*loaded, data);
        else             bettercppsax_parse(*loaded, no_alloc_data);

        parse_memory = current_memory;
    }
    follow_allocations = false;

    decltype(start_time) parse_ended_time = std::chrono::high_resolution_clock::now();

    //std::println("Done\n");

/*    std::println("Protocol:   {}", argv[1]);
    std::println("Source:     {}", argv[2]);
    std::println("Storage:    {}", argv[3]);
    std::println("File:       {}", argv[4]);
    */

    std::println("Load Time:  {}",
        std::chrono::duration_cast<std::chrono::milliseconds>(load_ended_time - start_time));
    std::println("Parse Time: {}",
        std::chrono::duration_cast<std::chrono::milliseconds>(parse_ended_time - load_ended_time)
    );

#ifdef TRACK_MEMORY
    std::println("Load Memory Allocations: {}",        load_memory.allocations_count);
    std::println("Load Max Live Allocations: {}",      load_memory.max_live_allocations);
    std::println("Load surviving Allocations: {}",     load_memory.live_allocations);
    std::println("Load Memory Allocated Size: {}",     load_memory.memory_allocated);
    std::println("Load Max Allocated Size: {}",        load_memory.max_memory_used);
    std::println("Load surviving Allocated Size: {}",  load_memory.memory_used);
    std::println("Parse Memory Allocations: {}",       parse_memory.allocations_count);
    std::println("Parse Max Live Allocations: {}",     parse_memory.max_live_allocations);
    std::println("Parse surviving Allocations: {}",    parse_memory.live_allocations);
    std::println("Parse Memory Allocated Size: {}",    parse_memory.memory_allocated);
    std::println("Parse Max Allocated Size: {}",       parse_memory.max_memory_used);
    std::println("Parse surviving Allocated Size: {}", parse_memory.memory_used);


    std::vector<std::pair<std::stacktrace, uint64_t>> stack_traces;

    for (const auto& item : allocations) stack_traces.push_back(item);


    std::ranges::sort(stack_traces, [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second;  });

    for (const auto& item : stack_traces) {
        if (item.second > 1) std::println("{} : {}\n\n==========================================\n", item.second, item.first);
    
    }
#endif
    return 0;
}