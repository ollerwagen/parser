#pragma once

#include "Grammar.hpp"
#include "Parser.hpp"

namespace cfg {

    class CYKParser : public Parser {

    private:

        GrammarManager gm;

    public:

        CYKParser();

        virtual void initGrammar(const std::string &filename);
        virtual bool parseInput(const std::vector<Terminal> &input);
    };

} // namespace cfg