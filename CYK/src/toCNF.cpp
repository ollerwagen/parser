#include <algorithm>
#include <deque>
#include <functional>

#include "Grammar.hpp"

// temporary
#include <iostream>

namespace cfg {

    static unsigned findMaxIndex(const Grammar &g) {
        Nonterminal res = 0;
        for (const auto &it : g) {
            res = std::max(res, it.first);

            for (const Rule &r : it.second) {
                for (const Symbol &s : r) {
                    if (!s.isTerminal) {
                        res = std::max(res, s.n);
                    }
                }
            }
        }

        return res;
    }

    // take S to S'
    // create new S with single rule S -> S'
    // Contract Assumption: g has a start symbol S
    // Contract Assumption: index >= max { nonterminals }
    static void replaceStart(Grammar &g, Nonterminal &index) {
        ++index;

        // S production rules to S'
        g[index] = g[S];
        g.erase(g.find(S));

        // S |-> S' in productions
        for (auto &it : g) {
            for (Rule &r : it.second) {
                for (Symbol &s : r) {
                    if (!s.isTerminal && s.n == S) {
                        s.n = index;
                    }
                }
            }
        }

        // create new start symbol S with single rule S -> S'
        g[S] = {{{{ false, { .n = index } }}}};
    }

    // replace all nonterminals 'a' with new nonterminals A with single rule A -> 'a'
    // this step isn't done for production rules of length 1 since these already 
    //   correspond to rules in CNF
    // Contract Assumption: index >= max { nonterminals }
    static void replaceTerminals(Grammar &g, Nonterminal &index) {
        std::map<Terminal, Nonterminal> newrule_mapping;

        // replace terminals with new pseudo-rules
        for (auto &it : g) {
            for (Rule &r : it.second) {
                if (r.size() <= 1) {
                    continue;
                }
                for (Symbol &s : r) {
                    if (s.isTerminal) {
                        if (newrule_mapping.find(s.t) == newrule_mapping.end()) {
                            newrule_mapping[s.t] = ++index;
                        }

                        s.isTerminal = false;
                        s.n = newrule_mapping[s.t];
                    }
                }
            }
        }

        // create actual productions from pseudo-rules
        for (const auto &it : newrule_mapping) {
            g[it.second].push_back({{{ true, { .t = it.first } }}});
        }
    }

    // replace all RHS rules with more than 2 nonterminals in it,
    // by replacing A -> X1 X2 ... Xn with A -> X1 Y1, Y1 -> X2 Y2, ..., Y(n-1) -> X(n-1) Xn
    // Contract Assumption: index >= max { nonterminals }
    static void refineRHS(Grammar &g, Nonterminal &index) {
        auto needsStep = [&] () -> bool {
            for (const auto &it : g) {
                for (const Rule &r : it.second) {
                    if (r.size() > 2) {
                        return true;
                    }
                }
            }
            return false;
        };

        while (needsStep()) {
            for (auto &it : g) {
                for (Rule &r : it.second) {
                    if (r.size() > 2) {
                        index++;
                        std::vector<Symbol> newrule(r.begin() + 1, r.end());
                        g[index] = { newrule };
                        r = { r.front(), { false, { .n = index } } };
                    }
                }
            }
        }
    }

    // remove all A -> € for A != S
    // for a nonterminal N, say N is nullable if either N -> € in P, or N -> A / AB with A nullable or A,B nullable
    // for all nonterminals with N -> A, A nullable, add rule N -> €,
    // for N -> AB, add rules N -> B for A nullable, N -> A for B nullable, and N -> € for both nullable
    // remove rules N -> € (unless N = S)
    // repeat this step until no nonterminal (except for S) is nullable
    // Contract Assumption: every production rule consists of -> €, a single symbol or two nonterminals
    static void eliminateEpsilon(Grammar &g) {
        std::map<Nonterminal, bool> called_nullable;

        for (const auto &it : g) {
            called_nullable[it.first] = false;
        }

        std::function<bool(Symbol)> nullable = [&] (Symbol s) -> bool {
            // terminal symbols are non-nullable by definition
            if (s.isTerminal) { return false; }

            Nonterminal n = s.n;

            // check if there is a recursive call -> if yes, say non-nullable
            if (called_nullable.at(n)) {
                return false;
            }

            called_nullable.at(n) = true;
            bool res = false;

            for (const Rule &r : g.at(n)) {
                for (const Symbol &s : r) {
                    if (!nullable(s)) {
                        goto AFTER_BREAK;
                    }
                }

                // all productions are nullable (or -> €, which is nullable)
                res = true;
                break;

                AFTER_BREAK:;
            }

            called_nullable.at(n) = false;
            return res;
        };

        auto needStep = [&] (void) -> bool {
            for (const auto &it : g) {
                if (it.first != S && std::find_if(it.second.begin(), it.second.end(),
                        [] (const Rule &r) -> bool { return r == Rule{}; }) != it.second.end()) {
                    return true;
                }
            }
            return false;
        };

        removeUselessRules(g);
        while (needStep()) {
            std::map<Nonterminal, bool> nullable_map;
            for (auto &it : g) {
                nullable_map[it.first] = nullable({ false, { .n = it.first } });
            }

            for (auto &it : g) {
                unsigned size = it.second.size();
                std::vector<Rule> &rules = it.second;
                for (unsigned i = 0; i < size; i++) {
                    switch (rules.at(i).size()) {
                        case 0: // € rule
                            if (it.first != S) {
                                rules.erase(rules.begin() + i);
                                i--, size--;
                            }
                            break;
                        case 1: // X -> a (do nothing) or X -> A
                            if (!rules.at(i).front().isTerminal) {
                                Symbol A = rules.at(i).front();
                                bool nA = nullable_map.at(A.n);
                                if (nA) {
                                    rules.push_back({});
                                }
                            }
                            break;
                        case 2: // X -> AB
                            {
                                Symbol A = rules.at(i).front(), B = rules.at(i).back();
                                bool nA = !A.isTerminal && nullable_map.at(A.n),
                                        nB = !B.isTerminal && nullable_map.at(B.n);

                                if (nA) { // A nullable: add X -> B
                                    rules.push_back({ { false, { .n = B.n } } });
                                }
                                if (nB) { // B nullable: add X -> A
                                    rules.push_back({ { false, { .n = A.n } } });
                                }
                                if (nA && nB) {
                                    rules.push_back({});
                                }
                            }
                            break;
                    }
                }
            }
            
            removeUselessRules(g);
        }
    }

    static void eliminateUnitRules(Grammar &g) {

        std::map<Nonterminal, bool> called, done;
        for(const auto &it : g) {
            called[it.first] = done[it.first] = false;
        }

        std::function<void(Nonterminal)> elim = [&] (Nonterminal n) -> void {

            if (called.at(n) || done.at(n)) {
                return;
            }

            std::vector<Rule> &rules = g.at(n);
            unsigned size = rules.size();
            for (unsigned i = 0; i < size; i++) {
                if (rules.at(i).size() == 1 && !rules.at(i).front().isTerminal) {
                    if (rules.at(i).front().n != n) {
                        elim(rules.at(i).front().n);
                        std::vector<Rule> newrules = g.at(rules.at(i).front().n);
                        rules.insert(rules.end(), newrules.begin(), newrules.end());
                    }
                    rules.erase(rules.begin() + i);
                    i--;
                }
            }
        };

        for (auto &it : g) {
            elim(it.first);
        }
        removeUselessRules(g);
    }

    GrammarManager GrammarManager::toCNF() const {
        Grammar g = this->g;

        Nonterminal nonterminal_index = findMaxIndex(g) + 1;

        replaceStart(g, nonterminal_index);
        replaceTerminals(g, nonterminal_index);
        refineRHS(g, nonterminal_index);
        eliminateEpsilon(g);
        eliminateUnitRules(g);

        GrammarManager res(g);
        res.nonterminal_maxindex = nonterminal_index;
        res.randgen_preferred = {};
        return res;
    }

} // namespace cfg