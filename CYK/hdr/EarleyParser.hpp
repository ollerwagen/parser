#pragma once

#include "Grammar.hpp"
#include "Parser.hpp"

namespace cfg {

    using Symbols = std::vector<Symbol>;
    using String = std::vector<Terminal>;
    constexpr Nonterminal ROOT = S - 1;

    struct ParseTree {
        Rule rule;
        std::vector<ParseTree> subtrees;
    };

    const ParseTree FAILED_PARSE = { Rule{}, std::vector<ParseTree>{} };

    class EarleyParser : public Parser {

    private:

        GrammarManager gm;

    public:

        EarleyParser();

        virtual void initGrammar(const std::string &filename);
        virtual bool parseInput(const String &input);
        std::pair<bool, ParseTree> parseTree(const String &input);
    };

} // namespace cfg