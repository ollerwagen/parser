#pragma once

#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Lexer.hpp"

namespace cfg {

    template<typename T>
    inline bool contains(const std::vector<T> &vec, const T &v) {
        for (const T &t : vec) {
            if (t == v) {
                return true;
            }
        }
        return false;
    }

    using Terminal = char;
    using Nonterminal = unsigned;

    constexpr Nonterminal S = 0, ROOT = -1, FAIL = -2;

    struct Symbol {
        bool isTerminal;
        union { Terminal t; Nonterminal n; };

        inline bool operator == (const Symbol &s) const {
            return isTerminal == s.isTerminal && (isTerminal ? (t == s.t) : (n == s.n));
        }
    };

    using Rule = std::vector<Symbol>;
    using Grammar = std::map<Nonterminal, std::vector<Rule>>;

    using Symbols = std::vector<Symbol>;
    using String = std::vector<Terminal>;

    struct ProductionTree {
        Nonterminal from;
        Rule rule;
        std::vector<ProductionTree> subtrees;
    };

    constexpr char CLEAR_RULE = '&', DELETE_RULE = '%';

    std::vector<Terminal> toTerminals(const std::string &s);

    std::map<Nonterminal, bool> getTerminateMap(const Grammar &g);

    class GrammarManager {

        friend class CYKParser;
        friend class EarleyParser;

    private:

        static const std::string START_SYMBOL;

    private:

        using omit = int8_t;

        Grammar g, fullresolveRules;
        std::unordered_map<Nonterminal, omit> clearProductions, deleteProductions;

        std::map<Nonterminal, bool> termination_map;

        std::map<std::string, Nonterminal> nonterminal_index_map;
        Nonterminal nonterminal_maxindex;

    private:

        Nonterminal addNonterminal(const std::string &name);
        std::vector<std::tuple<Token::Type, Nonterminal, Rule, char>> parseProductionRule(const std::string &line);

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

        GrammarManager toCNF() const;

        ProductionTree refineTree(const ProductionTree &tree);
        std::string printTree(const ProductionTree &tree, const std::string indent = "");

        std::string debugInfo();
    };

} // namespace cfg
