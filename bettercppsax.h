#pragma once

#include <variant>
#include <optional>
#include <stack>
#include <functional>
#include <iostream>
#include <format>
#include <cstdint>
#include <string_view>
#include <exception>
#include <charconv>

#include "rapidjson/reader.h"
#include "rapidjson/istreamwrapper.h"
//#include "yaml-cpp/parser.h"
//#include "yaml-cpp/anchor.h"
//#include "yaml-cpp/eventhandler.h"

/// <summary>
/// All possible types of JSON tokens, taken fron the underlying SAX reader.
/// </summary>
enum class JSONTokenType {
    null,
    boolean,
    number_integer,
    number_unsigned,
    number_float,
    string,
    start_object,
    end_object,
    start_array,
    end_array,
    key,
    parse_error,
};

// Every possible value type for a JSON item. so we can pass the value, with that information, as a single variable.
using json_val = std::variant<
    int64_t,
    uint64_t,
    double,
    std::string_view,
    bool>;

// A struct representing a single value - the token type, and the value, if it exists.
// "Key" token type is a string.
struct JSONToken {
    JSONTokenType type;
    json_val value;
};

/// <summary>
/// for specifying the handling of the next token.
/// </summary>
enum class ParseResultType {
    // Send the next token to the same parser.
    KeepParsing,
    // Send the next token to the previous parser, who submitted the current one.
    ParserDone,
    // Send the next token to the parser specified in the ParseResult
    NewParser,
    // Start using a new parser, starting with the current token rather than the next token.
    NewParser_ReplayCurrent,
    // An error has occured, stop parsing.
    Error
};

/// <summary>
/// Denotes the action to take after parsing a token. See ParseResultType for more details.
/// This struct adds the optional error or new parser.
/// </summary>
struct ParseResult {
    ParseResultType type;
    std::optional<std::string> error;
    std::optional<std::function<ParseResult(const JSONToken&)>> new_parser;

};

/// <summary>
/// A JSON SAX parser function type.
/// </summary>
using JSONParseFunc = std::function<ParseResult(const JSONToken&)>;

template <typename T>
using JSONObjectParser = std::function<ParseResult(std::string_view key, T& target)>;

[[nodiscard]]
inline ParseResult KeepParsing() {
    return ParseResult{ .type = ParseResultType::KeepParsing };
}
[[nodiscard]]
inline ParseResult ParserDone() {
    return ParseResult{ .type = ParseResultType::ParserDone };
}

[[nodiscard]]
inline ParseResult ParseError(std::string_view error) {
    return ParseResult{ .type = ParseResultType::Error, .error = (std::string)error };
}

[[nodiscard]]
inline ParseResult NewParser(const JSONParseFunc parser) {
    return ParseResult{ .type = ParseResultType::NewParser, .new_parser = parser };
}

[[nodiscard]]
inline ParseResult ParseString(std::string& target) {
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [&target](const JSONToken& token) mutable {
            if (token.type == JSONTokenType::string) {
                target = std::get<std::string_view>(token.value);
                return ParserDone();
            }
            else {
                return ParseError("Unexpected data type");
            }
         }
    };
}

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

[[nodiscard]]
inline ParseResult ParseNumber(Numeric auto& target) {
    using T = std::decay_t<decltype(target)>;
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [&target](const JSONToken& token) mutable {
            if (token.type == JSONTokenType::number_integer) {
                int64_t val = std::get<int64_t>(token.value);
                if (val > std::numeric_limits<T>::max() || val < std::numeric_limits<T>::lowest()) {
                    return ParseError("Number read is out of range for given type");
                }
                target = (T)val;
                return ParserDone();
            }
            else if (token.type == JSONTokenType::number_unsigned) {
                uint64_t val = std::get<uint64_t>(token.value);
                if (val > std::numeric_limits<T>::max()) {
                    return ParseError("Number read is out of range for given type");
                }
                target = (T)val;
                return ParserDone();
            }
            else if (token.type == JSONTokenType::number_float) {
                if (std::is_integral_v<std::decay_t<decltype(target)>>) {
                    return ParseError("Can't parse a floating point into an integral type");
                }
                double val = std::get<double>(token.value);
                if (val > std::numeric_limits<T>::max() || val < std::numeric_limits<T>::lowest()) {
                    return ParseError("Number read is out of range for given type");
                }

                target = (T)val;
                return ParserDone();
            }
            else if (token.type == JSONTokenType::string) {
                const auto& data = std::get<std::string_view>(token.value);
                if (std::from_chars(data.data(), data.data() + data.size(), target).ec == std::errc{}) return ParserDone();
                else return ParseError("Failed parsing integer");
            }
            else {
                return ParseError("Unexpected token type");
            }
         }
    };
}


[[nodiscard]]
inline ParseResult ParseBool(bool& target) {
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [&target](const JSONToken& token) mutable {
            if (token.type == JSONTokenType::boolean) {
                target = std::get<bool>(token.value);
                return ParserDone();
            }
            else {
                return ParseError("Unexpected data type");
            }
         }
    };
}

[[nodiscard]]
inline ParseResult SkipNextElement() {
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [depth = 0] (const JSONToken & token) mutable {
             switch (token.type) {
                 case JSONTokenType::start_object:
                 case JSONTokenType::start_array:
                     depth++;
                     break;
                 case JSONTokenType::end_object:
                 case JSONTokenType::end_array:
                     depth--;
                     break;
             }
             if (depth < 0) return ParseError("Malformed document while skiping element");
             return depth ? KeepParsing() : ParserDone();
        }
    };
}

template <class ContainerType>
concept SexContainer = requires(
    ContainerType & a,
    typename ContainerType::value_type const value_type
    )
{
    a.emplace_back();
    requires std::is_default_constructible_v<decltype(value_type)>;
};

template<typename COLLECTION>
requires SexContainer<COLLECTION>
[[nodiscard]]
inline ParseResult ParseList(COLLECTION& collection, std::function<JSONParseFunc(typename COLLECTION::value_type&)> parse_item_function) {
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [&collection, parse_item_function, first = true](const JSONToken& token) mutable {
            if (first) {
                first = false;
                if (token.type != JSONTokenType::start_array) return ParseError("No open array token for list");
                else return KeepParsing(); 
            }

            if (token.type == JSONTokenType::end_array) {
                return ParserDone();
            }

            return ParseResult{ .type = ParseResultType::NewParser_ReplayCurrent, .new_parser = parse_item_function(collection.emplace_back()) };
         }
    };
}

template<typename COLLECTION>
    requires SexContainer<COLLECTION>
[[nodiscard]]
inline ParseResult ParseList(COLLECTION& collection, std::function<ParseResult(typename COLLECTION::value_type&)> parse_item_function) {
        return ParseList(collection, [parse_item_function](std::string& target) { return parse_item_function(target).new_parser.value(); });
}

template<typename COLLECTION>
    requires SexContainer<COLLECTION>
[[nodiscard]]
inline ParseResult ParseObjectList(COLLECTION& collection, const JSONObjectParser<typename COLLECTION::value_type>& parse_item_function) {
    return ParseList(
        collection,
        [&, parse_item_function](typename COLLECTION::value_type& item) { return ParseObject([&, parse_item_function](std::string_view key) { return parse_item_function(key, item); });  }
    );
}


[[nodiscard]]
inline JSONParseFunc ParseObject(const std::function<ParseResult(std::string_view key)>& handler) {
    return [handler, first = true](const JSONToken& token) mutable {
        if (first) {
            first = false;
            if (token.type == JSONTokenType::start_object) return KeepParsing();
            else return ParseError("Expected object start");
        }
        else if (token.type == JSONTokenType::end_object) return ParserDone();
        else if (token.type == JSONTokenType::key) return handler(std::get<std::string_view>(token.value));
        else return ParseError("Unexpected element type");
    };
}

template<typename OBJECT>
[[nodiscard]]
inline ParseResult ParseObject(OBJECT& object, const JSONObjectParser<OBJECT>& handler) {
    return NewParser(
        ParseObject([&object, handler](std::string_view key) { return handler(key, object); })
    );
}


class SaxParser {
    struct inner_parser : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, inner_parser>/*, public YAML::EventHandler */ {
        explicit inner_parser(SaxParser& owner) : owner(owner) {}
    private:
        inline bool ParseToken(const JSONToken& token) {
            auto& cur_parser = owner.parser_stack.top();
            const auto& res = cur_parser(token);

            switch (res.type) {
            case ParseResultType::KeepParsing: return true;
            case ParseResultType::ParserDone: owner.parser_stack.pop(); return true;
            case ParseResultType::Error: owner.on_error(std::format("Bad JSON item: {}", res.error.value())); return false;
            case ParseResultType::NewParser: owner.parser_stack.push(res.new_parser.value()); return true;
            case ParseResultType::NewParser_ReplayCurrent: owner.parser_stack.push(res.new_parser.value());  return ParseToken(token);
            default: return false;
            }
        }
    public:

        // JSON parsing
        bool Null()                 { std::cout << "null\n"; return ParseToken({ .type = JSONTokenType::null }); }
        bool Bool(bool val)         { std::cout << "bool\n"; return ParseToken({ .type = JSONTokenType::boolean, .value = val }); }
        bool Int(int val)           { std::cout << "int\n"; return ParseToken({ .type = JSONTokenType::number_integer, .value = val }); }
        bool Int64(int64_t val)     { std::cout << "int64\n"; return ParseToken({ .type = JSONTokenType::number_integer, .value = val }); }
        bool UInt(unsigned int val) { std::cout << "uint\n"; return ParseToken({ .type = JSONTokenType::number_unsigned, .value = (uint64_t)val }); }
        bool UInt64(uint64_t val)   { std::cout << "uint64\n"; return ParseToken({ .type = JSONTokenType::number_unsigned, .value = val }); }
        bool Double(double val)     { std::cout << "double\n"; return ParseToken({ .type = JSONTokenType::number_float, .value = val }); }
        bool String(const char* str, rapidjson::SizeType length, bool copy) 
                                    { std::cout << "string\n"; return ParseToken({ .type = JSONTokenType::string , .value = std::string_view(str, length) }); }
        bool StartObject()          { std::cout << "SObj\n"; return ParseToken({ .type = JSONTokenType::start_object }); }
        bool EndObject(rapidjson::SizeType memberCount) 
                                    { std::cout << "EOnj\n"; return ParseToken({ .type = JSONTokenType::end_object }); }
        bool StartArray()           { std::cout << "SList\n"; return ParseToken({ .type = JSONTokenType::start_array }); }
        bool EndArray(rapidjson::SizeType memberCount) 
                                    { std::cout << "EList\n"; return ParseToken({ .type = JSONTokenType::end_array }); }
        bool Key(const char* str, rapidjson::SizeType length, bool copy) 
                                    { std::cout << "key\n"; return ParseToken({ .type = JSONTokenType::key , .value = std::string_view(str, length) }); }


        //// YAML parsing
        //void OnDocumentStart(const YAML::Mark& mark) final { ParseToken({ .type = JSONTokenType::start_object }); }
        //void OnDocumentEnd() final { ParseToken({ .type = JSONTokenType::end_object }); }
        //void OnNull(const YAML::Mark& mark, YAML::anchor_t anchor) final { ParseToken({ .type = JSONTokenType::null }); }
        //void OnAlias(const YAML::Mark& mark, YAML::anchor_t anchor) final { throw std::runtime_error("Aliases not currently supported"); }
        //void OnScalar(const YAML::Mark& mark, const std::string& tag, YAML::anchor_t anchor, const std::string& value) final { if (!tag.empty()) ParseToken({ .type = JSONTokenType::key, .value = value });  ParseToken({ .type = JSONTokenType::string }); }
        //void OnSequenceStart(const YAML::Mark& mark, const std::string& tag, YAML::anchor_t anchor, YAML::EmitterStyle::value style) final { if (!tag.empty()) ParseToken({ .type = JSONTokenType::key }); ParseToken({ .type = JSONTokenType::start_array }); }
        //void OnSequenceEnd() final { ParseToken({ .type = JSONTokenType::end_array }); }
        //void OnMapStart(const YAML::Mark& mark, const std::string& tag, YAML::anchor_t anchor, YAML::EmitterStyle::value style) final { if (!tag.empty()) ParseToken({ .type = JSONTokenType::key }); ParseToken({ .type = JSONTokenType::start_object }); }
        //void OnMapEnd() final { ParseToken({ .type = JSONTokenType::end_object }); }
        //void OnAnchor(const YAML::Mark& /*mark*/, const std::string& /*anchor_name*/) {   }


    private:
        SaxParser& owner;
    };

public:

    SaxParser() {};
    explicit SaxParser(const std::function<void(std::string_view)>& on_error) : on_error(on_error) {}


    void ParseJSON(std::istream& input, const JSONParseFunc& root_parser) {
        parser_stack.push(root_parser);
        rapidjson::Reader reader;
        auto wrapper = rapidjson::IStreamWrapper(input);
        inner_parser ip{ *this };
        reader.Parse(wrapper, ip);
    }

    template<typename T>
    void ParseJSON(std::istream& input, T& target, const std::function<ParseResult(std::string_view, T&)> parser) {
        return ParseJSON(input, [&](const JSONToken& token) {
            return ParseObject<T>(target, parser);
            });
    }

    //void ParseYAML(std::istream& input, const JSONParseFunc& root_parser) {
    //    parser_stack.push(root_parser);
    //    YAML::Parser parser(input);
    //    inner_parser ip{ *this };
    //    while (parser.HandleNextDocument(ip));
    //}

private:
    static void DefaultErrorHandler(std::string_view message) {
        std::cerr << "Error parsing JSON: " << message << std::endl;
    }

    std::stack<JSONParseFunc> parser_stack;
    std::function<void(std::string_view)> on_error = [](std::string_view error) { DefaultErrorHandler(error); };

};