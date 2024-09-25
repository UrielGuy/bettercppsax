# bettercppsax

This repository hold bettercppsax, a headers only C++23 library (other than its dependencies) to help build
efficient, easy and clean parsers for JSON files on top of industry standard SAX parsers. 

## What is this all about? 

Most JSON and YAML parsing libraries use the DOM operation mode. DOM mean loading all of the data in a file
into memory, in a relatively large data model. This both means holding all of the data in memory and at least 
two processing interations - one to load the JSON data, and one to go over the data structures. 
Alternatively, one can use the SAX parsing model.

The SAX Parsing model is a model where tokens are parsed by a parser, and then passed on to the user code
one by one. While fast and memory efficient, this requires the user to manage a lot of state in order to
parse effectively. This library helps abstract away all of the state managment, allowing engineers to write
fast, efficient parsing of JSON and YAML into C++ data types. 

## Abstraction Levels

### Easier Tokens

The two most common libraries with a JSON SAX interface (RapidJSON and Json for Modern C++)are made out of 
polymorphic interfaces where you are required to write a function for every possible token. This makes 
implementing even simple handlers (such as "Expect an int and save it to a variable") into a very tidious
task, as you have to implement a lot of methods you don't care about for a given state. It also means 
writing a type for each one of the states. Modern C++ allows us to use a different way of specifying a 
token, including a value, without resorting to OOP methods:

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
    std::monostate,
    int64_t,
    uint64_t,
    double,
    std::string_view,
    bool>;

struct JSONToken {
    JSONTokenType type;
    json_val value = std::monostate{};
};
```

This allows us to express every token as a single object with the token type and an optional 
value. Once a token is represented as an object, we can handle it quite simply. the approach
taken by this library is that a handler for a token should be a function that takes a token as
a parameter. 

### State Managment 

JSON is an hierarchical structure - you have objects and lists containing other objects, lists
and scalars. Logically it might also be recursive, as an object of a certain schema or type might
contain more objects of the same type. Hence, we need a datastructure that will allow us to save
parsers that we can go to, and put in new ones. We have chosen a stack, as we push new parsers, 
use them for a while, and then pop them out to return to the previous parser. 

Given a token, a parser can tell the machanism to perform one of 5 things: 

* Pass the next token to the same parser
* Pass the next token to a new parser specified by the current parser
* Pass the currenrt token again to a new parser specified by the current parser
* Finish its work, and pass the next token to the parser that created it. 
* Report an error and stop the parsing.

To accomplish all that, we created the following form for a parser: 

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

Every parser in its heart is a functor that takes a token, and return the next 
action. Now that we have the definition for a single parser, we can write the 
mechanism that manages a stream of tokens: 

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

This manages the stack of parsers based on the return value of each parser. However, writing
parsers using just this syntax is still clunky. As far as the parser logic manager is concerened,
we are done. It is now pussible to write full parsers, using features like designated initializers 
and mutable lambdas. An example parser might look like: 

```c++
std::string target;
auto parse_string = [&target](const JSONToken& token) {
    // Verify token type
    if (token.type == JSONTokenType::string) {
        // Parse the value
        target = std::get<std::string_view>(token.value);
        return ParseResult{.type = ParseResultType::ParserDone };
    }
    else {
        return ParseResult{.type = ParseResultType::Error, .error = "Unexpected data type"};
    }
}

auto my_type;

auto parse_my_type = [&my_type, first = false](const JSONToken& token) mutable {
    // Verify object is starting
    if (first) {
        first = false;
        if (token.type != JSONTokenType::start_object) return ParseResult { .type = ParseResultType::Error, .error = "Expected object start" };
        else return ParseResult { .type = ParseResultType::KeepParsing };
    }

    if (token.type == JSONTokenType::key) {
        const auto& key = std::get<std::string_view>(token.value);
        if (key == str) return ParseResult { .type = ParseResultType::NewParser, .new_parser = parse_string };
    }
    else if (token == JSONTokenType::end_object) {
        return ParseResult { .type = ParseResultType::ParsingDone };
    }
    else {
        return ParseResult { .type = ParseResultType::Error, .error = "Unexpected token type" };
    }
};
```

While fully capbale, this is not a great API. It's cumbursome and not very composable or reusable. 
Let's see how we can improve. 

### Let's make it all pretty

As much as this is already a powerful parser, it still requires a user to know quite a bit about the internal
structure of what we do, and some repetition does occur, such as defining the parsers quite often.  We can
add helper functions to help engineers write nicer code. At the end of the day, users of this library care
very little about JSON tokens and decisions about what to do next, they just want to be as expressive as 
possible about their types. Let's see how we can get them there. 

#### Hiding ParseResult

The first step is removing the explicit construction of ParseResult. We can add functions that will build them
and take the proper parameters for each type:

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

These clean up simple, default operations. This hides direct access to the `ParseResultType` enum, 
and makes it so that there is considerably less use of designated initializers, making the code cleaner.

#### Parsing Primitives

Given the base functions, and after parsing multiple primitives, these functionality can be reused.
Given this example for a string: 
```
[[nodiscard]]
inline auto ParseString(std::string& target) {
    return NewParser([&target](const JSONToken& token) mutable {
            if (token.type == JSONTokenType::string) {
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

This function is capable of parsing an string from a token and into a variable. similiar
functions can be written for any other primitive, such as integers, dates, floating point parameters, etc. 

However, even specifying what type you want to parse is already requiring the user to specify it twice - 
we should already know what is being parsed from the type of the variable. We can use `if constexpr` to 
route to the correct function: 

```c++
template<typename T>
[[nodiscard]]
inline auto ParseScalar(T& scalar) {     
    if constexpr (std::is_same_v<std::string, T>) return ParseString(scalar);
    else if constexpr (std::is_same_v<bool, T>) return ParseBool(scalar);
    else return ParseNumber(scalar);
}
```

It is possible to extend the library by writing more overrides of ParseScalar for other primitive types,
or call an explicit parser if needed.

#### Parsing objects

When parsing an object token by token, the workflow we suggest is asking the user what he wants to do for
every key while parsing a specific object. This translate into the following signature: 

```c++
template <typename T>
using JSONObjectParser = std::function<ParseResult(std::string_view key, T& target)>;
```

However, this doesn't handle actually handling tokens. For that we will create the ParseObject function: 
```c++
 template<typename OBJECT>
[[nodiscard]]
inline auto ParseObject(OBJECT& object, const core::JSONObjectParser<OBJECT>& handler) {
    return NewParser(
        [object, handler, first = true](const JSONToken& token) mutable {
            if (first) {
                first = false;
                if (token.type == JSONTokenType::start_object) return KeepParsing();
                else return ParseError("Expected object start");
            }
            else if (token.type == JSONTokenType::end_object) return ParserDone();
            else if (token.type == JSONTokenType::key) {
                return handler(std::get<std::string_view>(token.value), object);
            }
            else return ParseError("Unexpected element type");
        }
    );
}
```

This function switch the current parser to one that will verify we're actually parsing an object, and for
each key will consult the user provided function about what to do next.  This allows us to write a function
that parses a type that looks like this: 

```c++
auto ParseMyType(std::string_view key, my_type& target) {
    if (key == "address") return ParseScalar(target.address);
    else if (key == "id") return ParseScalar(target.id);
    else if (key == job) return ParseObject(target.job, ParseJob}
}
```

#### Parsing Lists

When parsing a list, there are a few things that need to happen:

* We need to verify it's actually a well structured list
* We need to allocate a new item to parse
* We need to parse the list item into the new item. 

We would like to create a function that takes in a collection, and adds an item for each item in the list. A 
useful feature of C++20 to use here is concepts. We can demand a collection that allows us to emplace an object
at the end, and that the object is default constructible. here is the concept: 

```c++
  template <class ContainerType>
    concept SexContainer = requires(
        ContainerType & a,
        typename ContainerType::value_type const value_type
        )
    {
        a.emplace_back();
            requires std::is_default_constructible_v<decltype(value_type)>;
    };
```

A list might also contain either scalars or objects. Hence, we would like to have two APIs: 
```c++
// Parse a list of scalars
 template<typename COLLECTION>
    requires SexContainer<COLLECTION>
[[nodiscard]]
inline auto ParseList(COLLECTION& collection, const JSONScalarParser<typename COLLECTION::value_type> get_item_parser_func = ParseScalar<typename COLLECTION::value_type>);

// Parse a list of objects. 
template<typename COLLECTION>
    requires SexContainer<COLLECTION>
[[nodiscard]]
inline auto ParseList(COLLECTION& collection, const JSONObjectParser<typename COLLECTION::value_type>& get_item_parser_func) {
```

Both of those call to an internal ParseList, which works with the base parser, that handles tokens directly. We can now parse all
all JSON types, doing thigs such as:


```c++
struct trade_type {
    double price;
    int size;
}

struct security {
    int id;
    std::vector<trade_type> trades;
    std:vector<int> numbers;
}

auto ParseTrade(std::string_view key, trade_type& trade) {
    if (key == "price") return ParseScalar(trade.price);
    else if (key == "size") return ParseSize(trade.size);
    else return ParseError("unexpected key in trade");
}

auto ParseSecurity(std::string_view, security& sec) {
    if (key == "id") return ParseScalar(sec.id);
    else if (key == "numbers") return ParseList(sec.numbers);
    else if (key == "trades") return ParseList(sec.trades, ParseTrade);
    else return ParseError("Unexpected key in security");
}

```
 
 This is getting lovely!
 
#### Skipping unneeded parts

The last missing piece is loosening the requirements on the structure of the JSON file by allowing us
to ignore unexpected data. We would like to be able to ignore unexpected sections rahter than failing
whenever we have any little surprise. In order to do this, we can write a function that will follow 
hierarchy of the JSON at a current spot and ignore it, with objects, lists or scalars. We can do this 
with the following function:

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

we can now change the `ParseTrade` function to the following: 

```c++
auto ParseTrade(std::string_view key, trade_type& trade) {
    if (key == "price") return ParseScalar(trade.price);
    else if (key == "size") return ParseSize(trade.size);
    else return SkipNextElement();
}
```

## Semantics

A good way of thinking how this looks with all these helper functions is that your code get called to handle an event you
wanted, and returns what shoukd happen next, in a way that builds a stack of handling - The next thing that might happen is that
an int will be parsed, or a string, or a list of something or an object. once that thing happens you get control back until you 
also decalre yourself as done. 

## Full example

Please look at the example directory in this repository
