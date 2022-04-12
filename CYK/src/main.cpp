#include <chrono>
#include <cstdlib>
#include <iostream>

#include "CYKParser.hpp"
#include "Grammar.hpp"

using namespace cfg;

int main(int argc, char **argv) {

    srand(std::chrono::system_clock::now().time_since_epoch().count());

    if (argc <= 1) {
        return 1;
    }

    CYKParser parser;
    parser.initGrammar(argv[1]);

    std::string line;

    do {
        std::getline(std::cin, line);
        std::cout << "Parse: " << (parser.parseInput(toTerminals(line)) ? "" : "un") <<
            "successful.\n";
    } while (!line.empty());

    /*GrammarManager gm;
    int repetitions = 1;

    if (argc > 1) {
        gm.parseFromFile(argv[1]);
        if (argc > 2) {
            try {
                repetitions = std::stoi(argv[2]);
            } catch (std::runtime_error &e) {}
        }
    } else {
        gm.parse({ "S -> A", "A -> B", "B -> C D", "C -> 'a'", "D -> S" });
    }

    auto grammar_check = gm.checkGrammar();
    if (!grammar_check.first) {
        std::cerr << "Bad Grammar: " << grammar_check.second << '\n';
    } else {
        std::cout << gm.debugInfo() << '\n';
        std::cout << "Grammar Terminates? " << (gm.terminates() ? "true" : "false") << '\n';
        std::cout << gm.randGen() << "\n\n";

        GrammarManager gmcnf = gm.toCNF();
        std::cout << gmcnf.debugInfo() << '\n';
        std::cout << "CNF Grammar Terminates? " << (gmcnf.terminates() ? "true" : "false") << '\n';
        
        for (int i = 0; i < repetitions; i++) {
            std::cout << gmcnf.randGen() << "\n";
        }
    }*/

    return 0;
}