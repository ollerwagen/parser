#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace cfg {

    struct Token {
        std::string text;
        enum class Type {
            STRING,
            NONTERMINAL,
            EPSILON,        // €
            PIPE,           // |
            ARROW,          // ->
            STAR_ARROW,     // *>
        } type;
    };

    std::vector<Token> lex(const std::string &line);

} // namespace cfg
