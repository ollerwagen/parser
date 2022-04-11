#pragma once

#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace cfg {

    using Terminal = char;
    using Nonterminal = unsigned;

    constexpr Nonterminal S = 0;

    struct Symbol {
        bool isTerminal;
        union { Terminal t; Nonterminal n; };

        inline bool operator == (const Symbol &s) const {
            return isTerminal == s.isTerminal && t == s.t && n == s.n;
        }
    };

    using Rule = std::vector<Symbol>;
    using Grammar = std::map<Nonterminal, std::vector<Rule>>;

    std::map<Nonterminal, bool> getTerminateMap(const Grammar &g);
    void removeUselessRules(Grammar &g);

    class GrammarManager {

    private:

        static constexpr unsigned MAX_RECURSION = 50;
        static const std::string START_SYMBOL;

        static constexpr double RANDGEN_START = 0.8;

        double randgen_preferred = RANDGEN_START;

    private:

        Grammar g, preferred_rules;

        std::map<Nonterminal, unsigned> terminating_paths;
        std::map<Nonterminal, bool> termination_map;

        std::map<std::string, Nonterminal> nonterminal_index_map;
        Nonterminal nonterminal_maxindex;

    private:

        Nonterminal addNonterminal(const std::string &name);
        std::vector<std::tuple<bool, Nonterminal, Rule>> parseProductionRule(const std::string &line);

        std::string getNonterminalName(Nonterminal n) const;

        void reset();

    public:

        GrammarManager();
        GrammarManager(const Grammar &g);
        GrammarManager(const GrammarManager &other);

        inline Grammar getGrammar() const { return g; }

        void parse(const std::vector<std::string> &lines);
        void parseFromFile(const std::string &filename);

        bool terminates();
        std::pair<bool, std::string> checkGrammar();

        void findTerminatingPath();

        GrammarManager toCNF() const;

        std::string debugInfo();
        std::string randGen(Nonterminal n = S, unsigned recursionDepth = 1);
    };

} // namespace cfg