/*
    An example parser to load dron shows, as described by the VVIZ format here:
    https://finale3d.com/documentation/vviz-file-format/#
*/
#include <fstream>
#include "../bettercppsax.h"

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

class VVIZParser {
private:
    static auto ParseTraversal(std::string_view key, drone_location_data& traversal) {
        if (key == "dx") return ParseFloat(traversal.location_delta.x);
        else if (key == "dy") return ParseFloat(traversal.location_delta.y);
        else if (key == "dz") return ParseFloat(traversal.location_delta.z);
        else if (key == "dt") { traversal.delay_seconds = 1.0; return ParseFloat(traversal.delay_seconds.value()); }
        else return ParseError("Unexpected key in traversal list");
    }

    static auto ParseAgentData(std::string_view key, drone_data& drone) {
        if (key == "homeX") return ParseFloat(drone.start_pos.x);
        else if (key == "homeY") return ParseFloat(drone.start_pos.y);
        else if (key == "homeZ") return ParseFloat(drone.start_pos.z);
        else if (key == "agentTraversal") return ParseObjectList(drone.agent_traversal, ParseTraversal);
        else return SkipNextElement();
    }

    static auto ParseDroneAction(std::string_view key, drone_action& action) {
        if (key == "r") return ParseFloat(action.color.r);
        else if (key == "g") return ParseFloat(action.color.g);
        else if (key == "b") return ParseFloat(action.color.b);
        else if (key == "frames") { action.frames = 1; return ParseUnsigned(action.frames.value()); }
        else return ParseError("Unexpected key in action ");
    }

    static auto ParseDronePayload(std::string_view key, drone_payload& payload) {
        if (key == "id") return ParseUnsigned(payload.id);
        else if (key == "type") return ParseString(payload.type);
        else if (key == "payloadActions") return ParseObjectList(payload.payload_actions, ParseDroneAction);
        else return SkipNextElement();
    }

    static auto ParsePerformance(std::string_view key, drone_data& drone) {
        if (key == "id") return ParseUnsigned(drone.id);
        else if (key == "agentDescription") return ParseObject<drone_data>(drone, ParseAgentData);
        else if (key == "payloadDescription") return ParseObjectList(drone.payload_actions, ParseDronePayload);
        else return SkipNextElement();
    }
public:
    static auto ParseRoot(std::string_view key, show_data& data)  {
        if (key == "version") return ParseString(data.version);
        else if (key == "defaultPositionRate") return ParseFloat(data.defaultPositionRate);
        else if (key == "defaultColorRate") return ParseFloat(data.defaultColorRate);
        else if (key == "timeOffsetSecs") return ParseFloat(data.timeOffsetSecs);
        else if (key == "performances") return ParseObjectList(data.performances, ParsePerformance);
        else return SkipNextElement();
    }
};

int main(int argc, char** argv) {
    show_data show;
    SaxParser parser;
    std::ifstream f(argv[1]);
    parser.ParseJSON(f,
        ParseObject([&show](std::string_view key) {return VVIZParser::ParseRoot(key, show); })
    );

}
