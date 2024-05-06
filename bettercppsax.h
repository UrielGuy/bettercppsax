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
#include "yaml-cpp/parser.h"
#include "yaml-cpp/anchor.h"
#include "yaml-cpp/eventhandler.h"

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

class SaxParser {
    struct inner_parser : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, inner_parser>, public YAML::EventHandler {
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
        bool Null() { return ParseToken({ .type = JSONTokenType::null }); }
        bool Bool(bool val) { return ParseToken({ .type = JSONTokenType::boolean, .value = val }); }
        bool Int(int val) { return ParseToken({ .type = JSONTokenType::number_integer, .value = val }); }
        bool Int64(int64_t val) { return ParseToken({ .type = JSONTokenType::number_integer, .value = val }); }
        bool UInt(unsigned int val) { return ParseToken({ .type = JSONTokenType::number_unsigned, .value = (uint64_t)val }); }
        bool UInt64(uint64_t val) { return ParseToken({ .type = JSONTokenType::number_unsigned, .value = val }); }
        bool Double(double val) { return ParseToken({ .type = JSONTokenType::number_float, .value = val }); }
        bool String(const char* str, rapidjson::SizeType length, bool copy) { return ParseToken({ .type = JSONTokenType::string , .value = std::string_view(str, length) }); }
        bool StartObject() { return ParseToken({ .type = JSONTokenType::start_object }); }
        bool EndObject(rapidjson::SizeType memberCount) { return ParseToken({ .type = JSONTokenType::end_object }); }
        bool StartArray() { return ParseToken({ .type = JSONTokenType::start_array }); }
        bool EndArray(rapidjson::SizeType memberCount) { return ParseToken({ .type = JSONTokenType::end_array }); }
        bool Key(const char* str, rapidjson::SizeType length, bool copy) { return ParseToken({ .type = JSONTokenType::key , .value = std::string_view(str, length) }); }


        // YAML parsing
        void OnDocumentStart(const YAML::Mark& mark) final {  ParseToken({ .type = JSONTokenType::start_object }); }
        void OnDocumentEnd() final {  ParseToken({ .type = JSONTokenType::end_object }); }
        void OnNull(const YAML::Mark& mark, YAML::anchor_t anchor) final {  ParseToken({ .type = JSONTokenType::null }); }
        void OnAlias(const YAML::Mark& mark, YAML::anchor_t anchor) final { throw std::runtime_error("Aliases not currently supported");  }
        void OnScalar(const YAML::Mark& mark, const std::string& tag, YAML::anchor_t anchor, const std::string& value) final { if (!tag.empty()) ParseToken({ .type = JSONTokenType::key, .value = value });  ParseToken({ .type = JSONTokenType::string }); }
        void OnSequenceStart(const YAML::Mark& mark, const std::string& tag, YAML::anchor_t anchor, YAML::EmitterStyle::value style) final { if (!tag.empty()) ParseToken({ .type = JSONTokenType::key}); ParseToken({ .type = JSONTokenType::start_array }); }
        void OnSequenceEnd() final {  ParseToken({ .type = JSONTokenType::end_array }); }
        void OnMapStart(const YAML::Mark& mark, const std::string& tag, YAML::anchor_t anchor, YAML::EmitterStyle::value style) final { if (!tag.empty()) ParseToken({ .type = JSONTokenType::key}); ParseToken({ .type = JSONTokenType::start_object }); }
        void OnMapEnd() final {  ParseToken({ .type = JSONTokenType::end_object }); }
        void OnAnchor(const YAML::Mark& /*mark*/, const std::string& /*anchor_name*/) {   }    


    private:
        SaxParser& owner;
    };
    
public: 

    SaxParser() {};
    explicit SaxParser(const std::function<void(std::string_view)>& on_error) : on_error(on_error) {}

    void Parse(std::istream& input, const JSONParseFunc& root_parser) {
        parser_stack.push(root_parser);
        rapidjson::Reader reader;
        auto wrapper = rapidjson::IStreamWrapper(input);
        inner_parser ip{ *this };
        reader.Parse(wrapper, ip);
    }

private:
    static void DefaultErrorHandler(std::string_view message) {
        std::cerr << "Error parsing JSON: " << message << std::endl;
    }

    std::stack<JSONParseFunc> parser_stack;
    std::function<void(std::string_view)> on_error = [](std::string_view error) { DefaultErrorHandler(error); };

};


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
                return ParseResult{.type = ParseResultType::ParserDone };
            }
            else {
                return ParseResult{.type = ParseResultType::Error, .error = "Unexpected data type"};
            }
         }
    };
}


template<typename T>
[[nodiscard]]
inline ParseResult ParseInt(T& target) {
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [&target](const JSONToken& token) mutable {
            if (token.type == JSONTokenType::number_integer) {
                target = std::get<int64_t>(token.value);
                return ParserDone();
            }
            else if (token.type == JSONTokenType::string) {
                const auto& data = std::get<std::string_view>(token.value);
                if (std::from_chars(data.data(), data.data() + data.size(), target).ec == std::errc{}) return ParserDone();
                else return ParseError("Failed parsing integer");
            }
            else {
                return ParseError("Unexpected data type" );
            }
         }
    };
}

template<typename T>
[[nodiscard]]
inline ParseResult ParseUnsigned(T& target) {
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [&target](const JSONToken& token) mutable {
            if (token.type == JSONTokenType::number_unsigned) {
                target = (T)std::get<uint64_t>(token.value);
                return ParseResult{.type = ParseResultType::ParserDone };
            }
            else if (token.type == JSONTokenType::string) {
                const auto& data = std::get<std::string_view>(token.value);
                if (std::from_chars(data.data(), data.data() + data.size(), target).ec == std::errc{}) return ParserDone();
                else return ParseError("Failed parsing integer");
            }
            else {
                return ParseResult{.type = ParseResultType::Error, .error = "Unexpected data type"};
            }
         }
    };
}

template<typename T>
[[nodiscard]]
inline ParseResult ParseFloat(T& target) {
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [&target](const JSONToken& token) mutable {
            if (token.type == JSONTokenType::number_integer) {
                target = (T)std::get<int64_t>(token.value);
                return ParserDone();
            }
            else if (token.type == JSONTokenType::number_unsigned) {
                target = (T)std::get<uint64_t>(token.value);
                return ParserDone();
            }
            else if (token.type == JSONTokenType::number_float) {
                target = (T)std::get<double>(token.value);
                return ParserDone();
            }
            else if (token.type == JSONTokenType::string) {
                const auto& data = std::get<std::string_view>(token.value);
                if (std::from_chars(data.data(), data.data() + data.size(), target).ec == std::errc{}) return ParserDone();
                else return ParseError("Failed parsing integer");
            }
            else {
                return ParseError("Unexpected data type");
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
             return ParseResult{.type = ((depth == 0) ? ParseResultType::ParserDone : ParseResultType::KeepParsing) };
        }
    };
}

template<typename COLLECTION>
[[nodiscard]]
inline ParseResult ParseList(COLLECTION& collection, std::function<JSONParseFunc(typename COLLECTION::value_type&)> parse_item_function) {
    return ParseResult{
        .type = ParseResultType::NewParser,
        .new_parser = [&collection, parse_item_function, first = true](const JSONToken& token) mutable {
            if (first) {
                first = false;
                if (token.type != JSONTokenType::start_array) return ParseResult{.type = ParseResultType::Error, .error = "No open array token for list" };
                else return ParseResult{.type = ParseResultType::KeepParsing };
            }

            if (token.type == JSONTokenType::end_array) {
                return ParseResult{.type = ParseResultType::ParserDone };
            }

            return ParseResult{ .type = ParseResultType::NewParser_ReplayCurrent, .new_parser = parse_item_function(collection.emplace_back()) };
         }
    };
}

template<typename COLLECTION>
[[nodiscard]]
inline ParseResult ParseObjectList(COLLECTION& collection, const std::function<ParseResult(std::string_view key, typename COLLECTION::value_type&)>& parse_item_function) {
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
