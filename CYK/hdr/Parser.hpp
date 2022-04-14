#include "Grammar.hpp"

namespace cfg {

    class Parser {

    public:

        virtual void initGrammar(const std::string &input) = 0;
        virtual bool parseInput(const std::vector<Terminal> &input) = 0;
    };
}