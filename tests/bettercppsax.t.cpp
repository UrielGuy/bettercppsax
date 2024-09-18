#include <catch2/catch_test_macros.hpp>
#include <bettercppsax.h>

using namespace bettercppsax;
using namespace bettercppsax::core;

TEST_CASE("ParseResult structure", "[ParseResult]") {
    SECTION("KeepParsing() returns correct ParseResult") {
        auto result = KeepParsing();
        REQUIRE(result.type == ParseResultType::KeepParsing);
        REQUIRE(!result.error.has_value());
        REQUIRE(!result.new_parser.has_value());
    }

    SECTION("ParserDone() returns correct ParseResult") {
        auto result = ParserDone();
        REQUIRE(result.type == ParseResultType::ParserDone);
        REQUIRE(!result.error.has_value());
        REQUIRE(!result.new_parser.has_value());
    }

    SECTION("ParseError() returns correct ParseResult") {
        auto result = ParseError("test error");
        REQUIRE(result.type == ParseResultType::Error);
        REQUIRE(result.error.has_value());
        REQUIRE(result.error.value() == "test error");
        REQUIRE(!result.new_parser.has_value());
    }

    SECTION("NewParser() returns correct ParseResult") {
        auto result = NewParser([](const JSONToken&) { return ParserDone(); });
        REQUIRE(result.type == ParseResultType::NewParser);
        REQUIRE(!result.error.has_value());
        REQUIRE(result.new_parser.has_value());
    }
}

TEST_CASE("Test ParseScalar", "[ParseScalar]") {
    std::string target;
    auto parse_res = ParseScalar(target);
    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);
    const auto& parser = parse_res.new_parser.value();

    SECTION("Good") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "Test String" });
        REQUIRE(target == "Test String");
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("Bad") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }
}

TEST_CASE("Test ParseScalar Int32", "[ParseScalar]") {
    int32_t target;
    auto parse_res = ParseScalar(target);

    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);
    const auto& parser = parse_res.new_parser.value();

    SECTION("positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_unsigned, .value = uint64_t(1234) });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_integer, .value = -1234 });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = 1234.0 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = -1234.0 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("string positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234.0" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234.0" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("Bad") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }
}

TEST_CASE("Test ParseScalar UInt32", "[ParseScalar]") {
    uint32_t target;
    auto parse_res = ParseScalar(target);

    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);
    const auto& parser = parse_res.new_parser.value();

    SECTION("positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_unsigned, .value = uint64_t(1234) });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_integer, .value = -1234 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = 1234.0 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = -1234.0 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("string positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234" });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("string positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234.0" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234.0" });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("Bad") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }
}

TEST_CASE("Test ParseScalar Double", "[ParseScalar]") {
    double target;
    auto parse_res = ParseScalar(target);

    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);
    const auto& parser = parse_res.new_parser.value();

    SECTION("positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_unsigned, .value = uint64_t(1234) });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_integer, .value = -1234 });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = 1234.0 });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = -1234.0 });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234.0" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234.0" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("Bad") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }
}

TEST_CASE("Test ParseScalar float", "[ParseScalar]") {
    float target;
    auto parse_res = ParseScalar(target);

    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);
    const auto& parser = parse_res.new_parser.value();

    SECTION("positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_unsigned, .value = uint64_t(1234) });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_integer, .value = -1234 });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = 1234.0 });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = -1234.0 });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234.0" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == 1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("string negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234.0" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(target == -1234);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("Bad") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }
}

TEST_CASE("Test ParseScalar Int8", "[ParseScalar]") {
    int8_t target;
    auto parse_res = ParseScalar(target);

    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);
    const auto& parser = parse_res.new_parser.value();

    SECTION("positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_unsigned, .value = uint64_t(1234) });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_integer, .value = -1234 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = 1234.0 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::number_float, .value = -1234.0 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("string positive integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234" });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("string negative integer") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234" });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("string positive float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "1234.0" });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("string negative float") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string, .value = "-1234.0" });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }

    SECTION("Bad") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }
}


TEST_CASE("Test ParseScalar", "[ParseScalar]") {
    bool target;
    auto parse_res = ParseScalar(target);
    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);
    const auto& parser = parse_res.new_parser.value();

    SECTION("Good true") {
        auto res = parser(JSONToken{ .type = JSONTokenType::boolean, .value = true });
        REQUIRE(target == true);
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("Good false") {
        auto res = parser(JSONToken{ .type = JSONTokenType::boolean, .value = false });
        REQUIRE(target == false);
        REQUIRE(res.type == ParseResultType::ParserDone);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == false);
    }

    SECTION("Bad") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.new_parser.has_value() == false);
        REQUIRE(res.error.has_value() == true);
    }
}


TEST_CASE("Test SkipElement", "[ParseSkipElement]") {
    auto parse_res = SkipNextElement();

    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);

    const auto& parser = parse_res.new_parser.value();

    SECTION("Simple value") {
        auto res = parser(JSONToken{ .type = JSONTokenType::boolean, .value = true });
        REQUIRE(res.type == ParseResultType::ParserDone);
    }

    SECTION("Empty list") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_array, .value = true });
        REQUIRE(res.type == ParseResultType::ParserDone);
    }

    SECTION("Empty object") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_object, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_object, .value = true });
        REQUIRE(res.type == ParseResultType::ParserDone);
    }

    SECTION("Nexted objects") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_object, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::start_object, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_object, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_object, .value = true });
        REQUIRE(res.type == ParseResultType::ParserDone);
    }

    SECTION("Nexted lists") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::start_array, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_array, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_array, .value = true });
        REQUIRE(res.type == ParseResultType::ParserDone);
    }

    SECTION("Nexted mixed") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_array, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::start_object, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_object, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_array, .value = true });
        REQUIRE(res.type == ParseResultType::ParserDone);
    }

    SECTION("Nexted mixed reversed") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_object, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::start_array, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_array, .value = true });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::end_object, .value = true });
        REQUIRE(res.type == ParseResultType::ParserDone);
    }

    SECTION("Malformed object") {
        auto res = parser(JSONToken{ .type = JSONTokenType::end_object, .value = true });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.error.has_value());
    }

    SECTION("Malformed list") {
        auto res = parser(JSONToken{ .type = JSONTokenType::end_array, .value = true });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.error.has_value());
    }
}

TEST_CASE("Parse Object", "[ParseObject]") {
    struct test_object {
        std::string str;
    } to;
    auto parse_res = ParseObject<test_object>(to, [](std::string_view key, test_object& to) {
        if (key == "str") return ParseScalar(to.str);
        else if (key == "error") return ParseError("Sending out an error");
        else return SkipNextElement();
    });

    REQUIRE(parse_res.type == ParseResultType::NewParser);
    REQUIRE(parse_res.error.has_value() == false);
    REQUIRE(parse_res.new_parser.has_value() == true);

    auto parser = parse_res.new_parser.value();

    SECTION("Valid object") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_object });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::key, .value = "str" });
        REQUIRE(res.type == ParseResultType::NewParser);
        res = res.new_parser.value()(JSONToken{ .type = JSONTokenType::string, .value = "str val" });
        REQUIRE(res.type == ParseResultType::ParserDone);
        res = parser(JSONToken{ .type = JSONTokenType::end_object });
        REQUIRE(res.type == ParseResultType::ParserDone);

        REQUIRE(to.str == "str val");
    }


    SECTION("Malformed object") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_object });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::number_integer, .value = 123 });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.error.has_value());
    }

    SECTION("Not an object") {
        auto res = parser(JSONToken{ .type = JSONTokenType::string });
        REQUIRE(res.type == ParseResultType::Error);
        REQUIRE(res.error.has_value());
    }

    SECTION("Internal error") {
        auto res = parser(JSONToken{ .type = JSONTokenType::start_object });
        REQUIRE(res.type == ParseResultType::KeepParsing);
        res = parser(JSONToken{ .type = JSONTokenType::key, .value = "error" });
        REQUIRE(res.type == ParseResultType::Error);
    }
}

TEST_CASE("Parse List", "[ParseList]") {
    std::vector<std::string> values;

    auto parse_res = ParseList(values);
    
}

/*
TEST_CASE("SaxParser class", "[SaxParser]") {
    SECTION("ParseJSON() parses JSON correctly") {
        std::string json = R"({
    "name": "John", 
    "age": 123456789012345,
    "asda": "sdfs"
})";
        std::istringstream iss(json);
        SaxParser parser;

        std::string targetName;
        int targetAge = 0;

        // Define your root parser function
        JSONParseFunc rootParser = ParseObject([&](std::string_view key) -> ParseResult {
            if (key == "name") {
                return ParseScalar(targetName);
            }
            else if (key == "age") {
                return ParseScalar(targetAge);
            }
            else {
                return SkipNextElement();
            }
            });

        parser.ParseJSON(iss, rootParser);

        // Add assertions to verify parsed values
        REQUIRE(targetName == "John");
        REQUIRE(targetAge == 30);
    }

    // Add more tests to cover various scenarios of JSON and YAML parsing
}

*/