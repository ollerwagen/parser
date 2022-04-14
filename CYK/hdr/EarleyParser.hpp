#pragma once

#include "Grammar.hpp"
#include "Parser.hpp"

namespace cfg {

    class EarleyParser : public Parser {

    private:

        GrammarManager gm;

    public:

        EarleyParser();

        virtual void initGrammar(const std::string &input);
        virtual bool parseInput(const std::vector<Terminal> &input);
    };

} // namespace cfg