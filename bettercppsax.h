// Copyright 2024 Uriel Guy
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
#include <expected>
#include <utility>

#include "rapidjson/reader.h"
#include "rapidjson/istreamwrapper.h"

namespace bettercppsax {

    namespace core {
        // All possible types of JSON tokens, taken fron the underlying SAX reader.
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
            std::monostate,
            int64_t,
            uint64_t,
            double,
            std::string_view,
            bool>;

        // A struct representing a single value - the token type, and the value, if it exists.
        // "Key" token type is a string.
        struct JSONToken {
            JSONTokenType type;
            json_val value = std::monostate{};
        };

        // for specifying the handling of the next token.
        enum class ParseResultType {
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

        // Denotes the action to take after parsing a token. See ParseResultType for more details.
        // This struct adds the optional error or new parser.
        struct ParseResult {
            ParseResultType type;
            std::optional<std::string> error;
            std::optional<std::function<ParseResult(const JSONToken&)>> new_parser;

        };

        // A JSON SAX parser function type.
        using JSONParseFunc = std::function<ParseResult(const JSONToken&)>;

        template <typename T>
        using JSONObjectParser = std::function<ParseResult(std::string_view key, T& target)>;

/*        template <typename T>
        using JSONScalarParser = std::function<ParseResult(T& target)>;
        */

        template <class ContainerType>
        concept SaxContainer = requires(
            ContainerType & a,
            typename ContainerType::value_type const value_type
            )
        {
            a.emplace_back();
            requires std::is_default_constructible_v<decltype(value_type)>;
        };

        class SaxParser {
            struct inner_parser {
                explicit inner_parser(SaxParser& owner) : owner(owner) {}
            private:

                inline bool ParseToken(const JSONToken& token) {
                    auto res = owner.parser_stack.top()(token);

                    switch (res.type) {
                    case ParseResultType::KeepParsing:  return true;
                    case ParseResultType::ParserDone:  owner.parser_stack.pop(); return true;
                    case ParseResultType::Error: owner.on_error(std::format("Bad Token: {}", res.error.value()));  return false;
                    case ParseResultType::NewParser: owner.parser_stack.emplace(std::move(res.new_parser.value())); return true;
                    case ParseResultType::NewParser_ReplayCurrent: 
                        owner.parser_stack.emplace(std::move(res.new_parser.value()));  
                        return ParseToken(token);
                    default: 
                        std::unreachable();
                    }
                }
            public:

                // JSON parsing
                bool Null() { 
                    return ParseToken({ .type = JSONTokenType::null }); 
                }
                bool Bool(bool val) { 
                    return ParseToken({ .type = JSONTokenType::boolean,           .value = val }); 
                }
                bool Int(int val) {
                    return ParseToken({ .type = JSONTokenType::number_integer,    .value = val });
                }
                bool Int64(int64_t val) { 
                    return ParseToken({ .type = JSONTokenType::number_integer,    .value = val });
                }
                bool Uint(unsigned int val) { 
                    return ParseToken({ .type = JSONTokenType::number_unsigned,   .value = (uint64_t)val });
                }
                bool Uint64(uint64_t val) { 
                    return ParseToken({ .type = JSONTokenType::number_unsigned,   .value = val });
                }
                bool Double(double val) { 
                    return ParseToken({ .type = JSONTokenType::number_float,      .value = val });
                }
                bool String(const char* str, rapidjson::SizeType length, bool copy) {
                    return ParseToken({ .type = JSONTokenType::string ,           .value = std::string_view(str, length) });
                }
                bool RawNumber(const char* str, rapidjson::SizeType length, bool copy) {
                    return ParseToken({ .type = JSONTokenType::string ,           .value = std::string_view(str, length) });
                }
                bool StartObject() {
                    return ParseToken({ .type = JSONTokenType::start_object });
                }
                bool EndObject(rapidjson::SizeType memberCount) { 
                    return ParseToken({ .type = JSONTokenType::end_object });
                }
                bool StartArray() { 
                    return ParseToken({ .type = JSONTokenType::start_array });
                }
                bool EndArray(rapidjson::SizeType memberCount) { 
                    return ParseToken({ .type = JSONTokenType::end_array });
                }
                bool Key(const char* str, rapidjson::SizeType length, bool copy) { 
                    return ParseToken({ .type = JSONTokenType::key ,              .value = std::string_view(str, length) });
                }

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
                reader.Parse<
                    rapidjson::ParseFlag::kParseNoFlags 
                    | rapidjson::ParseFlag::kParseDefaultFlags 
                    | rapidjson::ParseFlag::kParseNumbersAsStringsFlag 
//                    | rapidjson::ParseFlag::kParseInsituFlag 
                >
                    (wrapper, ip);
            }

        private:
            static void DefaultErrorHandler(std::string_view message) {
                throw std::runtime_error(std::format("Failed parsing JSON with the followind error:\n{}", message));
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
    }
    [[nodiscard]]
    inline core::ParseResult ParseError(std::string_view error) {
        return core::ParseResult{ .type = core::ParseResultType::Error, .error = (std::string)error };
    }

    namespace core {
        [[nodiscard]]
        inline ParseResult NewParser(JSONParseFunc&& parser) {
            return ParseResult{ .type = ParseResultType::NewParser, .new_parser = std::move(parser) };
        }
       
        [[nodiscard]]
        inline ParseResult NewParserRepeatToken(JSONParseFunc&& parser) {
            return ParseResult{ .type = ParseResultType::NewParser_ReplayCurrent, .new_parser = std::move(parser) };
        }
       
        [[nodiscard]]
        inline ParseResult ParseString(std::string& target) {
            return NewParser([&target](const JSONToken& token) mutable {
                if (token.type == JSONTokenType::string) {
                    target = std::get<std::string_view>(token.value);
                    return ParserDone();
                }
                else {
                    return ParseError("Unexpected data type");
                }
            });
        }
       
        template <typename T>
        concept Numeric = std::integral<T> || std::floating_point<T>;
       
        [[nodiscard]]
        inline ParseResult ParseNumber(Numeric auto& target) {
            using T = std::decay_t<decltype(target)>;
            return NewParser([&target](const JSONToken& token) mutable {
                if (token.type == JSONTokenType::string) {
                    const auto& data = std::get<std::string_view>(token.value);
                    // Improve comparison once C++26 is available
                    if (std::from_chars(data.data(), data.data() + data.size(), target).ec == std::errc{}) return ParserDone();
                    else return ParseError("Failed parsing number");
                }
                else {
                    return ParseError("Unexpected token type");
                }
            });
        }


        [[nodiscard]]
        inline ParseResult ParseBool(bool& target) {
            return NewParser([&target](const JSONToken& token) mutable {
                if (token.type == JSONTokenType::boolean) {
                    target = std::get<bool>(token.value);
                    return ParserDone();
                }
                else {
                    return ParseError("Unexpected data type");
                }
            });
        }

        template<typename COLLECTION>
            requires SaxContainer<COLLECTION>
        [[nodiscard]]
        inline ParseResult ParseList(
            COLLECTION& collection,
            std::function<JSONParseFunc(typename COLLECTION::value_type&)>&& parser_factory)
        {
            return NewParser([&collection, parser_factory = std::move(parser_factory), first = true](const JSONToken& token) mutable {
                if (first) {
                    first = false;
                    if (token.type != JSONTokenType::start_array) return ParseError("No open array token for list");
                    else return KeepParsing();
                }
                else if (token.type == JSONTokenType::end_array) {
                    return ParserDone();
                }
                else {
                    return NewParserRepeatToken(parser_factory(collection.emplace_back()));
                }
                });
        }

        template<typename T>
        [[nodiscard]]
        inline ParseResult ParseList(
            std::function<JSONParseFunc(T&)>&& parser_factory)
        {
            return NewParser([item = T{}, parser_factory = std::move(parser_factory), first = true](const JSONToken& token) mutable {
                if (first) {
                    first = false;
                    if (token.type != JSONTokenType::start_array) return ParseError("No open array token for list");
                    else return KeepParsing();
                }
                else if (token.type == JSONTokenType::end_array) {
                    return ParserDone();
                }
                else {
                    item = T{};
                    return NewParserRepeatToken(parser_factory(item));
                }
            });
        }
    } // namespace core

    template<typename T>
    [[nodiscard]]
    inline auto ParseScalar(T& scalar) {     
        using namespace core;

        if constexpr (std::is_same_v<std::string, T>) return ParseString(scalar);
        else if constexpr (std::is_same_v<bool, T>) return ParseBool(scalar);
        else return ParseNumber(scalar);
    }

    [[nodiscard]]
    inline auto SkipNextElement() {
        using namespace core;
        return NewParser([depth = 0](const JSONToken& token) mutable {
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
            return depth > 0 ? KeepParsing() : ParserDone();
        });
    }

    template<typename OBJECT>
    [[nodiscard]]
    inline auto ParseObject(
        OBJECT& object, 
        const core::JSONObjectParser<OBJECT>& handler, 
        std::function<bool(const OBJECT&)> validator = [](const OBJECT&) { return true; }) 
    {
        using namespace core;
        return NewParser(
            [validator = std::move(validator), &object, handler, first = true](const JSONToken& token) mutable {
                if (first) {
                    first = false;
                    if (token.type == JSONTokenType::start_object) return KeepParsing();
                    else return ParseError("Expected object start");
                }
                else if (token.type == JSONTokenType::end_object) {
                    if (validator(object))  return ParserDone();
                    else return ParseError("Invalid object");
                }
                else if (token.type == JSONTokenType::key) {
                    return handler(std::get<std::string_view>(token.value), object);
                }
                else return ParseError("Unexpected element type");
            }
        );
    }
    using namespace core;

    template <typename T>
    using JSONScalarParser = std::function<ParseResult(T& target)>;

    // ParseList for Scalars
    template<typename COLLECTION>
        requires SaxContainer<COLLECTION>
    [[nodiscard]]
    inline auto ParseList(
            COLLECTION& collection, 
            const JSONScalarParser<typename COLLECTION::value_type>& item_parser = ParseScalar<typename COLLECTION::value_type>) 
    {
        return ParseList(
                    collection, 
                    [item_parser = std::move(item_parser)](typename COLLECTION::value_type& target) { 
                        return item_parser(target).new_parser.value(); 
            });
    }

    // Parse List for Objects
    template<typename COLLECTION>
        requires SaxContainer<COLLECTION>
    [[nodiscard]]
    inline auto ParseList(
        COLLECTION& collection, 
        const JSONObjectParser<typename COLLECTION::value_type>& item_parser,
        std::function<bool(const typename COLLECTION::value_type&)>&& validator 
            = [](const typename COLLECTION::value_type&) { return true; }) 
    {
        return ParseList(
            collection,
            [item_parser = std::move(item_parser), validator = std::move(validator)](typename COLLECTION::value_type& target) {
                return ParseObject(target, item_parser, validator).new_parser.value();
            }
        );
    }

    // Parse List for Objects
    template<typename T>
    [[nodiscard]]
    inline auto ParseList(
        const JSONObjectParser<typename T>& item_parser,
        std::function<bool(const T&)> validator) 
    {
        using namespace core;
        return ParseList(
            [item_parser = std::move(item_parser)](T& target) {
                return ParseObject(target, item_parser, validator).new_parser.value();
            }
        );
    }

    /// Ovveride for the parse function that returns an optional error string if parsing failed.
    template<typename T>
    std::expected<void, std::string> ParseJson(std::istream& stream, T& object, const core::JSONObjectParser<T> root_parser) {
        std::optional<std::string> res;
        core::SaxParser parser([&res](std::string_view error) {res = error; });
        parser.ParseJSON(stream, ParseObject<T>(object, root_parser).new_parser.value());
        if (res.has_value()) return std::unexpected(res.value());
        else return std::expected<void, std::string>{};
    }

    template<typename T>
    std::expected<void, std::string> ParseJson(std::istream&& stream, T& object, const core::JSONObjectParser<T> root_parser) {
        return ParseJson((std::istream &)stream, object, root_parser);
    }


    
    /// Ovveride for the parse function that returns an optional error string if parsing failed.
    template<typename T>
    std::expected<T, std::string> ParseJson(std::istream& stream, const core::JSONObjectParser<T> root_parser) {
        static_assert(std::is_default_constructible_v<T>, "To use the overload that doesn't take an object, the type parsed must be default constructible");
        T object;
        std::optional<std::string> res;
        core::SaxParser parser([&res](std::string_view error) {res = error; });
        parser.ParseJSON(stream, ParseObject<T>(object, root_parser).new_parser.value());;
        if (res.has_value()) {
            return std::unexpected(res.value());
        }
        else {
            return object;
        }
    }

    template<typename T>
    std::expected<T, std::string> ParseJson(std::istream&& stream, const core::JSONObjectParser<T> root_parser) {
        return ParseJson((std::istream &)stream, root_parser);
    }

}// namespace bettercppsax
