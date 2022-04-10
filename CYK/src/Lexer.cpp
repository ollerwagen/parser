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

    std::vector<Token> lex(const std::string &line) {
        static const std::regex regex("((€)|(\\|)|(\\->)|(\\*>)|(\"(([^\"\\\\€]|\\\\\"|\\\\'|\\\\\\\\|\\\\n|\\\\t)*)\"|'(([^'\\\\€]|\\\\'|\\\\\"|\\\\\\\\|\\\\n|\\\\t)*)')|(\\w+)|(#[\\w\\d]*))");

        std::smatch match;
        std::vector<Token> tokens;

        std::string cur = line;

        while (std::regex_search(cur, match, regex)) {
            std::string elem = match.str();
            cur = match.suffix();
            if (elem.at(0) == '#') { // ignore comment
                continue;
            } else if (elem == "€") {
                tokens.push_back({ elem, Token::Type::EPSILON });
            } else if (elem == "->") {
                tokens.push_back({ elem, Token::Type::ARROW });
            } else if (elem == "*>") {
                tokens.push_back({ elem, Token::Type::PREFER_ARROW });
            } else if (elem == "|") {
                tokens.push_back({ elem, Token::Type::PIPE });
            } else if (elem.find_first_of("\"'") == std::string::npos) {
                tokens.push_back({ elem, Token::Type::NONTERMINAL });
            } else {
                tokens.push_back({ cleanupString(elem), Token::Type::STRING });
            }
        }

        return tokens;
    }

} // namespace cfg