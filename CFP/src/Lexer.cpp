#include <algorithm>
#include <regex>

#include "Lexer.hpp"

// temporary
#include <iostream>

namespace cfg {

    static std::string cleanupString(const std::string &str) {
        std::string res = str.substr(1, str.length() - 2);

        for (unsigned i = 0; i < res.length(); i++) {
            if (res.at(i) == '\\') {
                switch (res.at(i + 1)) {
                    case '\\': res.replace(i, 2, "\\"); break;
                    case '\'': res.replace(i, 2, "'");  break;
                    case '\"': res.replace(i, 2, "\""); break;
                    case 'n':  res.replace(i, 2, "\n"); break;
                    case 't':  res.replace(i, 2, "\t"); break;
                }
            }
        }

        return res;
    }

    static bool containsOnlyWhitespace(const std::string &between_string) {
        static const std::string WHITESPACE = " \t";
        return std::find_if(between_string.begin(), between_string.end(),
            [] (const char c) { return WHITESPACE.find(c) == std::string::npos; }) == between_string.end();
    }

    std::vector<Token> lex(const std::string &line) {
        static const std::regex regex("((€)|(\\|)|(\\->)|(\\*>)|(\"(([^\"\\\\€]|\\\\\"|\\\\'|\\\\\\\\|\\\\n|\\\\t)*)\"|'(([^'\\\\€]|\\\\'|\\\\\"|\\\\\\\\|\\\\n|\\\\t)*)')|([\\&\\%]?\\w+)|(#[\\w\\d]*))");

        std::smatch match;
        std::vector<Token> tokens;

        std::string cur = line;

        while (std::regex_search(cur, match, regex)) {
            std::string elem = match.str();

            std::string between_string = cur.substr(0, cur.find(elem));
            if (!containsOnlyWhitespace(between_string)) {
                throw std::runtime_error("Lexer Error at '" + between_string + "'.");
            }

            cur = match.suffix();

            if (elem.at(0) == '#') { // ignore comment
                continue;
            } else if (elem == "€") {
                tokens.push_back({ elem, Token::Type::EPSILON });
            } else if (elem == "->") {
                tokens.push_back({ elem, Token::Type::ARROW });
            } else if (elem == "*>") {
                tokens.push_back({ elem, Token::Type::STAR_ARROW });
            } else if (elem == "|") {
                tokens.push_back({ elem, Token::Type::PIPE });
            } else if (elem.find_first_of("\"'") == std::string::npos) {
                tokens.push_back({ elem, Token::Type::NONTERMINAL });
            } else {
                tokens.push_back({ cleanupString(elem), Token::Type::STRING });
            }
        }

        if (!containsOnlyWhitespace(cur)) {
            throw std::runtime_error("Lexer Error at '" + cur + "'.");
        }

        return tokens;
    }

} // namespace cfg
