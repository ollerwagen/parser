#pragma once

#include "Grammar.hpp"

namespace cfg {

    class CYKParser {

    private:

        GrammarManager gm;

    public:

        CYKParser();

        void initGrammar(const std::string &filename);
        bool parseInput(const std::vector<Terminal> &input);
    };

} // namespace cfg