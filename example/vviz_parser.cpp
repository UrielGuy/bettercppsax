/*
    An example parser to load dron shows, as described by the VVIZ format here:
    https://finale3d.com/documentation/vviz-file-format/#
*/
#include <fstream>
#include "../bettercppsax.h"

using namespace bettercppsax;

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

struct drone_payload {
    int id;
    std::string type;
    std::vector<drone_action> payload_actions;
};


struct drone_data {
    int id;
    XYZ_t start_pos;
    std::vector<drone_location_data> agent_traversal;
    std::vector<drone_payload>  payload_actions;
};

struct show_data {
    std::string version;
    double defaultPositionRate;
    double defaultColorRate;
    double timeOffsetSecs;
    std::vector<drone_data> performances;

};
auto ParseRGB(RGB_t& rgb) {
    using namespace bettercppsax::core;

    return NewParser([&rgb](const JSONToken& token) {
        if (token.type == JSONTokenType::string) {
            auto& str = std::get<std::string_view>(token.value);
            if (!str.starts_with("#")) return core::ParseError("Invalid string for color");
            uint32_t value = std::strtol(str.data() + 1, nullptr, 16);
            rgb.r = (value & 0xFF0000) >> 16;
            rgb.g = (value & 0xFF00) >> 8;
            rgb.b = (value & 0xFF);
            return ParserDone();
        }
        else if (token.type == JSONTokenType::start_array) {
            return NewParser([&rgb, index = 0](const JSONToken& token) mutable {
                if (index == 3) {
                    if (token.type == JSONTokenType::end_array) return ParserDone();
                    else return core::ParseError("Unexpected type");
                }
                else if (index++ == 0) return NewParserRepeatToken(ParseScalar(rgb.r).new_parser.value());
                else if (index++ == 1) return NewParserRepeatToken(ParseScalar(rgb.g).new_parser.value());
                else if (index++ == 2) return NewParserRepeatToken(ParseScalar(rgb.b).new_parser.value());
                // Should be std::unreachable in C++23
                else return core::ParseError("Should never get here");
                } );
        }
        else if (token.type == JSONTokenType::start_object) {
            return NewParser([&rgb, index = 0](const JSONToken& token) mutable {
                if (index == 3) {
                    if (token.type == JSONTokenType::end_object) return ParserDone();
                    else return core::ParseError("Unexpected type");
                }
                else if (token.type != JSONTokenType::key) return core::ParseError("Unexpected token");
                auto key = std::get<std::string_view>(token.value);
                if (key == "r") return ParseScalar(rgb.r);
                else if (key == "g") return ParseScalar(rgb.g);
                else if (key == "b") return ParseScalar(rgb.b);
                else return core::ParseError("Unexpected value");
                });

        }
        else return core::ParseError("Unexpected token type");
    });
}


class VVIZParser {
private:
    static auto ParseTraversal(std::string_view key, drone_location_data& traversal) {
        if (key == "dx") return ParseScalar(traversal.location_delta.x);
        else if (key == "dy") return ParseScalar(traversal.location_delta.y);
        else if (key == "dz") return ParseScalar(traversal.location_delta.z);
        else if (key == "dt") { traversal.delay_seconds = 1.0; return ParseScalar(traversal.delay_seconds.value()); }
        else return ParseError("Unexpected key in traversal list");
    }

    static auto ParseAgentData(std::string_view key, drone_data& drone) {
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

    static auto ParseDronePayload(std::string_view key, drone_payload& payload) {
        if (key == "id") return ParseScalar(payload.id);
        else if (key == "type") return ParseScalar(payload.type);
        else if (key == "payloadActions") return ParseList(payload.payload_actions, ParseDroneAction);
        else return SkipNextElement();
    }

    static auto ParsePerformance(std::string_view key, drone_data& drone) {
        if (key == "id") return ParseScalar(drone.id);
        else if (key == "agentDescription") return ParseObject<drone_data>(drone, ParseAgentData);
        else if (key == "payloadDescription") return ParseList(drone.payload_actions, ParseDronePayload);
        else return SkipNextElement();
    }
public:
    static auto ParseRoot(std::string_view key, show_data& data)  {
        if (key == "version") return ParseScalar(data.version);
        else if (key == "defaultPositionRate") return ParseScalar(data.defaultPositionRate);
        else if (key == "defaultColorRate") return ParseScalar(data.defaultColorRate);
        else if (key == "timeOffsetSecs") return ParseScalar(data.timeOffsetSecs);
        else if (key == "performances") return ParseList(data.performances, ParsePerformance);
        else return SkipNextElement();
    }
};

int main(int argc, char** argv) {
    // Will be replaced with std::Expected for C++23.
    auto parse_res = bettercppsax::ParseJson<show_data>(std::ifstream(argv[1]), VVIZParser::ParseRoot);
    
    if (std::holds_alternative<std::string>(parse_res)) {
        std::cout << "Failed parsing with error:\n" << std::get<std::string>(parse_res) << std::endl;
        return -1;
    }
    
    show_data& data = std::get<show_data>(parse_res);
}
