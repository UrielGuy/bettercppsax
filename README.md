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

It is now pussible to write full parsers, using features like designated initializers. An example parser might look like: 

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

