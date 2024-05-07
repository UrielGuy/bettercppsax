# bettercppsax

This repository hold bettercppsax, a headers only C++20 library (other than its dependencies) to help build
efficient, easy and clean parsers for JSON and YAML files on top of industry standard SAX parsers. 

## What is this all about? 

Most JSON and YAML parsing libraries use the DOM operation mode. DOM mean loading all of the data in a file
into memory, in a relatively large data model. This both means holding all of the data in memory and at least 
two processing interations - one to load the JSON/YAML data, and one to go over the data structures. 
Alternatively, one can use the SAX parsing model.

The SAX Parsing model is a model where tokens are parsed by a parser, and then passed on to the user code
one by one. While fast and memory efficient, this requires the user to manage a lot of state to be able to
parse effectively. This library helps abstract away all of the state managment, allowing engineers to write
fast, efficient parsing of JSON and YAML into C++ data types. 

## Abstraction Levels

### Easier Tokens

Most APIs for SAX parsing is made out ouf polymorphic interfaces. Event handlers are usually a class 
implementing a method for each possible event. This makes implementing a specific handler noisy, as you
have to implement a lot of methods you don't care about for the current state. It also means there's a type 
for each one of the states. 

To improve on this, We have create the `JSONToken` type, defined here: 

```c++
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

using json_val = std::variant<
    int64_t,
    uint64_t,
    double,
    std::string_view,
    bool>;

struct JSONToken {
    JSONTokenType type;
    json_val value;
};
```

This allows us to express every token as a single variable, so every state can be expressed as a method, taking
the current token as a parameter.  These are also relatively cheap and can be passed by value. We are now only required to create a single "native" event handler, and translate all events into this structure. 

### State Managment 

One assumption made is that the parsing is somewhat recursive - an object can contain other instances of itself (making simple state machines harder to use for this), but also simplifying the set of events that need to be currently handled. This means that we can create a stack of parsers, where  a token is parsed, the parser can do one of 5 things: 

* Ask to also receive the next token as well
* Ask to pass the next token to a new parser pushed to the stack
* Ask to pass the currenrt token again to a new parser pushed to the stack
* Ask to finish its role and pass the next token back to the previous parser on the stack
* Report an error and stop the parsing.

Given all that, a parser is just a functor of the following form: 

```c++

enum class ParseResultType {
    KeepParsing,
    NewParser,
    NewParser_ReplayCurrent,
    ParserDone,
    Error
};

struct ParseResult {
    ParseResultType type;
    std::optional<std::string> error;
    std::optional<std::function<ParseResult(const JSONToken&)>> new_parser;
};

using JSONParseFunc = std::function<ParseResult(const JSONToken&)>;
```

This allows us to write the actual core logic in the following form: 

```c++
class Parser {
    std::stack<JSONParseFunc> parser_stack;
public:
     bool ParseToken(const JSONToken& token) {
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

};
```

As far as the parser logic manager is concerened, we are done. It is now pussible to write full parsers, using features like designated initializers and mutable lambdas. An example parser might look like: 

```c++
ParseResult ParseString(const JSONToken& token, std::string& target) {
    if (token.type == JSONTokenType::string) {
        target = std::get<std::string_view>(token.value);
        return ParseResult{.type = ParseResultType::ParserDone };
    }
    else {
        return ParseResult{.type = ParseResultType::Error, .error = "Unexpected data type"};
    }
}

ParseResult ParseObject (const JSONtoken& token, object& target) {
    if (token.type == JSONTokenType::start_object) {
         return ParseResult { .type = ParseResultType::KeepParsing; }
    }
    else if (token.type == JSONTokenType::key) {
        const auto& key = std::get<std::string_view>(target.value);
        if (key == "prop1") return ParseResult {
            .type = ParseResultType::NewParser,
            .new_parser = [&target](const JSONToken& token) mutable { ParseString(toekn, target.prop1); }
        };
        else if (key == "prop2") return ParseResult {
            .type = ParseResultType::NewParser,
            .new_parser = [&target](const JSONToken& token) mutable { ParseString(toekn, target.prop1); }
        };

    }
    else if (token.type == JSONTokenType::end_object) {
        return ParseResult { .type = ParseResultType::DoneParsing; }
    }
    else {
       return ParseResult { .type = ParseResultType::Error, .error = "Unexpected token type" }; 
    }
}
```

### Let's make it all pretty

As much as this is already a powerful parser, it still requires a user to know quite a bit about the internal
structure of what we do, and some repetision does occur, such as defining the parsers quite often.  We can
add helper functions to help engineers write nicer code. The final goal is to completely hide `JSONToken`
from them, and hide ParseResult in `auto` types. 

#### Hiding ParseResult

The first step to remove redundant code is to add function to make creating the objects easier:
```C++
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

```

These clean up simple, default operations. This hides direct access to the `ParseResultType` enum, and makes it so that there is considerably less use of designated initializers, making the code cleaner.

#### Parsing Primitives

Given the base functions, and after parsing multiple primitives, these functionality can be reused. Given this example for an int: 
```
template<typename T>
[[nodiscard]]
inline ParseResult ParseInt(T& target) {
    return NewParser([&target](const JSONToken& token) mutable {
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
    );
}
```

This function is capable of parsing an integer from either a string or an integer primitive. similiar
functions can be written for any other primitive, such as a string, a date, a floating point number, etc. 
The library provides:
* `ParseInteger`
* `ParseUnsigned`
* `ParseFloat`
* `ParseString`
* `ParseBool`

If a user is intereseted in extanding the library externally, it is remarkebly easy as these are all just
functions. One can easily add `ParseDate` or `ParseFormula`.

#### Parsing types

Parsing a type, usually represented as an object or a dictionary in the markup, required some state managment - mostly when does it start, when are keys legal, and when does it end. We have decided to help our users by supplying the following function: 

```c++
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
```

This function takes in another function that takes a key to a dictionary, and returns a parse result, and uses it inside of a parser function that manages the state of parsing an object, and makes it much easier toparse the object. Given all of our facilities we can now write the following: 

```c++
auto ParseObject(std::string_view key, object& target) {
    if (key == "address") return ParseString(target.address);
    else if (key == "id") return ParseUnsigned(target.id);
    else if (key == job) return NewParser(ParseObject([&job = target.job](string_view key) mutable { return ParseJob(key, job); }
}
```

#### Parsing Lists

When parsing a list, we need to allocate new objects as they appear, and then parse them. We also need to understand when is the end of the list and stop parsing. This is done by the following functions: 

```c++
template<typename COLLECTION>
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
```

To make it easier to parse a list of objects, We supply the following function on top of the previous one: 

```c++

template<typename COLLECTION>
[[nodiscard]]
inline ParseResult ParseObjectList(COLLECTION& collection, const std::function<ParseResult(std::string_view key, typename COLLECTION::value_type&)>& parse_item_function) {
    return ParseList(
        collection,
        [&, parse_item_function](typename COLLECTION::value_type& item) { return ParseObject([&, parse_item_function](std::string_view key) { return parse_item_function(key, item); });  }
    );
}
```

This brings us almost to the code in the example folder, with one more thing to go.

#### Skipping unneeded parts

As we must process ALL tokens in a document, it might be that there are parts we do not care about. Some keys in a dictionary that
are not interesting, or optional parts we have no need for. For that we supply the following function: 

```c++

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
             return depth ? KeepParsing() : ParserDone();
        }
    };
}
```

## Semantics

A good way of thinking how this looks with all these helper functions is that your code get called to handle an event you
wanted, and returns what shoukd happen next, in a way that builds a stack of handling - The next thing that might happen is that
an int will be parsed, or a string, or a list of something or an object. once that thing happens you get control back until you 
also decalre yourself as done. 

## Full example

Please look at the example directory in this repository
