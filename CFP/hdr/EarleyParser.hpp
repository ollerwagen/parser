#pragma once

#include "Grammar.hpp"
#include "Parser.hpp"

namespace cfg {

    const ProductionTree FAILED_PARSE = { FAIL, Rule{}, std::vector<ProductionTree>{} };

    class EarleyParser : public Parser {

    private:

        GrammarManager gm;

    public:

        EarleyParser();

        virtual void initGrammar(const std::string &filename);
        virtual bool parseInput(const String &input);
        std::pair<bool, ProductionTree> parseTree(const String &input);
    };

} // namespace cfg