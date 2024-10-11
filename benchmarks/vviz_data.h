#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <string>



struct RGB_t {
    uint8_t r, g, b;
};

struct XYZ_t {
    double x, y, z;
};

struct drone_location_data {
    XYZ_t location_delta;
    std::optional<double> delay_seconds;
};

struct drone_action {
    RGB_t color;
    std::optional<int> frames;
};

template< template<typename> typename COLLECTION>
struct drone_payload {
    int id;
    std::string type;
    COLLECTION<drone_action> payload_actions;
};


template< template<typename> typename COLLECTION>
struct drone_data {
    int id;
    XYZ_t start_pos;
    COLLECTION<drone_location_data> agent_traversal;
    COLLECTION<drone_payload<COLLECTION>>  payload_actions;
};

template< template<typename> typename COLLECTION>
struct show_data {
    std::string version;
    double defaultPositionRate;
    double defaultColorRate;
    double timeOffsetSecs;
    COLLECTION<drone_data<COLLECTION>> performances;

};          