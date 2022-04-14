#include <vector>

#include "EarleyParser.hpp"

// temporary
#include <iostream>

namespace cfg {

    struct EarleyItem {
        unsigned start, end;
        std::vector<Symbol> pre, post;
    };

    EarleyParser::EarleyParser() {}

    void EarleyParser::initGrammar(const std::string &filename) {
        gm.parseFromFile(filename);
        gm = gm.toCNF();

        std::cout << gm.debugInfo() << "\n\n";
    }

    bool EarleyParser::parseInput(const std::vector<Terminal> &input) {
        std::vector<EarleyItem> items;

        
    }

} // namespace cfg