#include <array>
#include <vector>

#include "CYKParser.hpp"

// temporary
#include <iostream>
#include <sstream>

namespace cfg {

    template<typename T>
    static bool contains(const std::vector<T> &v, T n) {
        for (const T nt : v) {
            if (n == nt) {
                return true;
            }
        }
        return false;
    }

    static std::string printDPTable(const std::vector<std::vector<std::vector<Nonterminal>>> &DP) {
        std::stringstream stream;

        for (unsigned x = 0; x < DP.size(); x++) {
            for (unsigned y = 0; y <= x; y++) {
                if (DP.at(x).at(y).empty()) {
                    stream << "*\t";
                } else {
                    for (unsigned i = 0; i < DP.at(x).at(y).size(); i++) {
                        stream << DP.at(x).at(y).at(i) << ',';
                    }
                    stream << '\t';
                }
            }
            stream << '\n';
        }
        return stream.str();
    }

    CYKParser::CYKParser() {}

    void CYKParser::initGrammar(const std::string &filename) {
        gm.parseFromFile(filename);
        gm = gm.toCNF();

        std::cout << gm.debugInfo() << "\n\n";
    }

    bool CYKParser::parseInput(const std::vector<Terminal> &input) {

        if (input.empty()) {
            return contains(gm.g.at(S), Rule{});
        }
        
        std::vector<std::vector<std::vector<Nonterminal>>> DP;

        unsigned n = input.size();

        DP.resize(n);
        for (unsigned i = 0; i < n; i++) {
            DP.at(i).resize(i + 1);
        }

        // initialize DP table
        for (unsigned i = 0; i < n; i++) {
            for (auto &it : gm.g) {
                for (const Rule &r : it.second) {
                    if (r.size() == 1 && r.front() == Symbol{ true, { .t = input.at(i) } } &&
                            !contains(DP.at(i).at(i), it.first)) {
                        DP.at(i).at(i).push_back(it.first);
                    }
                }
            }
        }

        // fill DP table
        for (unsigned i = 1; i < n; i++) {
            for (int j = i; j >= 0; j--) {
                for (unsigned d = 1; d <= i - j; d++) {
                    const std::vector<Nonterminal> &vi = DP.at(i - d).at(j), &vj = DP.at(i).at(i - d + 1);

                    for (const Nonterminal &na : vi) {
                        for (const Nonterminal &nb : vj) {
                            for (const auto &it : gm.g) {
                                Rule r = { Symbol{ false, { .n = na } }, Symbol{ false, { .n = nb } } };
                                if (contains(it.second, r) && !contains(DP.at(i).at(j), it.first)) {
                                    DP.at(i).at(j).push_back(it.first);
                                }
                            }
                        }
                    }
                }
            }
        }

        std::cout << "\n\n" << printDPTable(DP) << "\n\n";

        return contains(DP.at(n - 1).at(0), S);
    }

} // namespace cfg