#include <chrono>
#include <cstdlib>
#include <iostream>

#include "CYKParser.hpp"
#include "EarleyParser.hpp"
#include "Grammar.hpp"

using namespace cfg;

int main(int argc, char **argv) {

    srand(std::chrono::system_clock::now().time_since_epoch().count());

    if (argc <= 2) {
        std::cout << "Enter the desired parsing schema first (-cyk or -earley), followed by the grammar file.\n";
        return 1;
    }

    Parser *parser = nullptr;

    if (argv[1] == std::string("-cyk")) {
        parser = new CYKParser();
    } else if (argv[1] == std::string("-earley")) {
        parser = new EarleyParser();
    } else {
        std::cout << "Invalid Parsing Schema.\n";
        return 2;
    }

    try {
        parser->initGrammar(argv[2]);
    } catch (std::runtime_error &e) {
        std::cout << "Grammar Parse Error: " << e.what() << '\n';
    }

    std::string line;
    do {
        std::getline(std::cin, line);
        std::cout << "Parse " << (parser->parseInput(toTerminals(line)) ? "" : "un") << "successful\n";
    } while (!line.empty());

    return 0;
}