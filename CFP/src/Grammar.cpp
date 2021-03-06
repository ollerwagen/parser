#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
#include <sstream>

#include "Grammar.hpp"

// temporary
#include <iostream>

namespace cfg {

    static const std::tuple<Token::Type, Nonterminal, Rule, char> FAIL_RULE = { Token::Type::ARROW, UINT_MAX, {}, '\0' };

    static bool containsNoCode(const std::string &line) {
        static const std::string WHITESPACE = " \t";

        for (unsigned i = 0; i < line.length(); i++) {
            if (line.at(i) == '#') { return true; }
            if (WHITESPACE.find_first_of(line.at(i)) == std::string::npos) { return false; }
        }
        return true;
    }

    std::vector<Terminal> toTerminals(const std::string &s) {
        std::vector<Terminal> res;
        res.resize(s.length());

        for (unsigned i = 0; i < s.length(); i++) {
            res.at(i) = s.at(i);
        }

        return res;
    }

    const std::string GrammarManager::START_SYMBOL = "S";

    std::map<Nonterminal, bool> getTerminateMap(const Grammar &g) {

        std::map<Nonterminal, bool> called, known, termination_map;
        for (const auto &it : g) {
            called[it.first] = known[it.first] = termination_map[it.first] = false;
        }

        std::function<bool(const Symbol &s)> term = [&] (const Symbol &s) -> bool {
            if (s.isTerminal) { return true; }

            Nonterminal n = s.n;

            if (called.at(n)) { return false; }
            if (known.at(n)) { return termination_map.at(n); }

            termination_map.at(n) = false;
            called.at(n) = true;

            for (const Rule &rule : g.at(n)) {
                bool success = true;
                for (const Symbol &sym : rule) {
                    if (!term(sym)) {
                        success = false;
                        break;
                    }
                }

                if (success) {
                    termination_map.at(n) = true;
                    break;
                }
            }

            known.at(n) = true;
            called.at(n) = false;
            return termination_map.at(n);
        };

        for (const auto &it : g) {
            term({ false, { .n = it.first } });
        }

        return termination_map;
    }

    Nonterminal GrammarManager::addNonterminal(const std::string &name) {
        if (nonterminal_index_map.find(name) != nonterminal_index_map.end()) {
            return nonterminal_index_map.at(name);
        } else {
            nonterminal_index_map[name] = nonterminal_maxindex;
            return nonterminal_maxindex++;
        }
    }

    std::vector<std::tuple<Token::Type, Nonterminal, Rule, char>> GrammarManager::parseProductionRule(const std::string &line) {
        std::vector<Token> tokens = lex(line);
        std::size_t index = 0;

        auto inBounds = [&] () -> bool  { return index < tokens.size(); };
        auto peek     = [&] () -> Token { return tokens.at(index); };
        auto advance  = [&] () -> Token { return tokens.at(index++); };
        auto expect   = [&] (Token::Type type) -> Token {
            if (peek().type == type) { return advance(); }
            throw std::runtime_error("Expected different token.");
        };
        auto select   = [&] (int count, ...) -> Token {
            va_list va;
            va_start(va, count);
            for (int i = 0; i < count; i++) {
                if (peek().type == va_arg(va, Token::Type)) {
                    va_end(va);
                    return advance();
                }
            }
            va_end(va);
            throw std::runtime_error("Expected Tokens not found.");
        };

        try {
            Token nonterminal_token = expect(Token::Type::NONTERMINAL);

            const char first = nonterminal_token.text.front();
            if (first == CLEAR_RULE || first == DELETE_RULE) {
                nonterminal_token.text = nonterminal_token.text.substr(1);
            }

            Nonterminal rule_index = addNonterminal(nonterminal_token.text);
            Token::Type arrowType = select(2, Token::Type::ARROW, Token::Type::STAR_ARROW).type;

            std::vector<std::tuple<Token::Type, Nonterminal, Rule, char>> res;

            while (inBounds()) {
                if (peek().type == Token::Type::EPSILON) {
                    advance();
                    res.push_back({ arrowType, rule_index, {}, first });
                    if (inBounds()) { expect(Token::Type::PIPE); }
                    continue;
                }

                std::vector<Token> symbol_tokens;
                while (inBounds() && peek().type != Token::Type::PIPE) {
                    symbol_tokens.push_back(select(2, Token::Type::NONTERMINAL, Token::Type::STRING));
                }

                if (inBounds() && peek().type == Token::Type::PIPE) {
                    advance();
                }

                std::vector<Symbol> rule_production;
                for (const Token &t : symbol_tokens) {
                    if (t.type == Token::Type::STRING) {
                        for (unsigned i = 0; i < t.text.length(); i++) {
                            rule_production.push_back({ true, { .t = t.text.at(i) } });
                        }
                    } else { // Token::Type::NONTERMINAL
                        rule_production.push_back({ false, { .n = addNonterminal(t.text) } });
                    }
                }

                res.push_back({ arrowType, rule_index, rule_production, first });
            }

            return res;

        } catch (std::runtime_error &e) {
            std::cerr << "Error in line '" << line << "': " << e.what() << '\n';
            return { FAIL_RULE };
        }
    }

    std::string GrammarManager::getNonterminalName(Nonterminal n) const {
        for (const auto &it : nonterminal_index_map) {
            if (it.second == n) { return it.first; }
        }
        return std::to_string(n);
    }

    void GrammarManager::reset() {
        g.clear();
        fullresolveRules.clear();

        clearProductions.clear();
        deleteProductions.clear();

        nonterminal_index_map[START_SYMBOL] = 0;
        nonterminal_maxindex = 1;
    }

    GrammarManager::GrammarManager() {
        reset();
    }

    GrammarManager::GrammarManager(const Grammar &g) {
        reset();
        this->g = g;

        checkGrammar();
    }

    GrammarManager::GrammarManager(const GrammarManager &other) {
        g = other.g;
        fullresolveRules = other.fullresolveRules;

        nonterminal_index_map = other.nonterminal_index_map;
        nonterminal_maxindex = other.nonterminal_maxindex;
    }

    void GrammarManager::parse(const std::vector<std::string> &lines) {
        reset();

        unsigned line_number = 1;
        for (std::string line : lines) {
            if (containsNoCode(line)) {
                continue;
            }

            std::vector<std::tuple<Token::Type, Nonterminal, Rule, char>> newrules = parseProductionRule(line);

            for (const auto &r : newrules) {
                if (r == FAIL_RULE) {
                    std::cerr << "Error in Line " << line_number << '\n';
                } else {
                    const Token::Type type = std::get<0>(r);
                    const Nonterminal from = std::get<1>(r);
                    const Rule rule = std::get<2>(r);
                    const char first = std::get<3>(r);
                    g[from].push_back(rule);
                    if (type == Token::Type::STAR_ARROW) {
                        fullresolveRules[from].push_back(rule);
                    }

                    switch (first) {
                        case CLEAR_RULE:
                            clearProductions[from] = 0;
                            break;
                        case DELETE_RULE:
                            deleteProductions[from] = 0;
                            break;
                    }
                }
            }

            line_number++;
        }

        std::pair<bool, std::string> grammarCheck = checkGrammar();
        if (!grammarCheck.first) {
            throw std::runtime_error(grammarCheck.second);
        }
    }

    void GrammarManager::parseFromFile(const std::string &filename) {
        std::fstream file(filename);

        if (!file) {
            return;
        }

        std::vector<std::string> lines;
        std::string tmp;
        while (!file.eof()) {
            std::getline(file, tmp);
            lines.push_back(tmp);
        }

        parse(lines);
    }

    bool GrammarManager::terminates() {
        std::map<Nonterminal, bool> known, called;

        termination_map.clear();

        for (const auto &rule : g) {
            known[rule.first] = false;
            called[rule.first] = false;
            termination_map[rule.first] = false;
        }

        std::function<bool(Symbol)> term = [&] (Symbol s) -> bool {
            if (s.isTerminal) { return true; }

            Nonterminal n = s.n;

            if (called.at(n)) { return false; }
            if (known.at(n)) { return termination_map.at(n); }

            termination_map.at(n) = false;
            called.at(n) = true;

            for (const Rule &rule : g.at(n)) {
                bool success = true;
                for (const Symbol &sym : rule) {
                    if (!term(sym)) {
                        success = false;
                        break;
                    }
                }

                if (success) {
                    termination_map.at(n) = true;
                    break;
                }
            }

            known.at(n) = true;
            called.at(n) = false;
            return termination_map.at(n);
        };

        for (auto &it : g) {
            term({ false, { .n = it.first } });
        }

        return termination_map.at(S);
    }

    std::pair<bool, std::string> GrammarManager::checkGrammar() {
        for (const auto &it : g) {
            for (const Rule &r : it.second) {
                for (const Symbol &s : r) {
                    if (!s.isTerminal && g.find(s.n) == g.end()) {
                        return { false, "Nonterminal '" + getNonterminalName(s.n) + "' undefined." };
                    }
                }
            }
        }

        if (terminates()) {
            return { true, "" };
        } else {
            return { false, "Grammar doesn't terminate." };
        }
    }

    ProductionTree GrammarManager::refineTree(const ProductionTree &tree) {        
        ProductionTree tmp = { tree.from, tree.rule, std::vector<ProductionTree>{} };

        for (const ProductionTree &t : tree.subtrees) {
            if (clearProductions.find(t.from) != clearProductions.end() ||
                    deleteProductions.find(t.from) != deleteProductions.end()) {
                tmp.subtrees.push_back(ProductionTree{ t.from, Rule{}, std::vector<ProductionTree>{} });
            } else {
                tmp.subtrees.push_back(refineTree(t));
            }
        }
        
        ProductionTree res = { tmp.from, Rule{}, std::vector<ProductionTree>{} };

        if (fullresolveRules.find(tmp.from) != fullresolveRules.end() &&
                contains(fullresolveRules.at(tmp.from), tmp.rule)) {
            unsigned subtree_i = 0;
            for (const Symbol &s : tmp.rule) {
                if (s.isTerminal) {
                    res.rule.push_back(s);
                } else {
                    const ProductionTree &subtree = tmp.subtrees.at(subtree_i++);
                    res.rule.insert(res.rule.end(), subtree.rule.begin(), subtree.rule.end());
                    res.subtrees.insert(res.subtrees.end(), subtree.subtrees.begin(), subtree.subtrees.end());
                }
            }
        } else {
            res = tmp;
        }

        tmp = res;
        res = { tmp.from, Rule{}, std::vector<ProductionTree>{} };

        unsigned subtree_i = 0;
        for (const Symbol &s : tmp.rule) {
            if (s.isTerminal) {
                res.rule.push_back(s);
            } else {
                if (deleteProductions.find(s.n) == deleteProductions.end()) {
                    res.rule.push_back(s);
                    res.subtrees.push_back(tmp.subtrees.at(subtree_i));
                }
                subtree_i++;
            }
        }

        return res;
    }

    std::string GrammarManager::printTree(const ProductionTree &tree, const std::string indent) {
        std::stringstream stream;

        stream << indent << getNonterminalName(tree.from) << " -> ";
        if (tree.rule.empty()) {
            stream << "???";
        } else {
            stream << "'";
            for (const Symbol &s : tree.rule) {
                if (s.isTerminal) {
                    stream << "\033[31m" << s.t << "\033[0m";
                } else {
                    stream << "<" << getNonterminalName(s.n) << ">";
                }
            }
            stream << "'";
        }

        stream << ":\n";
        
        unsigned subtree_ptr = 0;
        for (const Symbol &s : tree.rule) {
            if (s.isTerminal) {
                stream << indent << s.t << '\n';
            } else {
                if (subtree_ptr < tree.subtrees.size()) {
                    stream << printTree(tree.subtrees.at(subtree_ptr++), indent + "    ");
                } else {
                    stream << indent << "    <missing tree!!>\n";
                }
            }
        }

        return stream.str();
    }

    std::string GrammarManager::debugInfo() {
        std::stringstream stream;

        terminates();

        for (auto it = g.begin(); it != g.end(); it++) {
            for (const Rule &r : it->second) {
                stream << "[" << it->first << ": " << getNonterminalName(it->first) << "] -> ";
                if (r.empty()) {
                    stream << "???";
                } else {
                    for (const Symbol &s : r) {
                        if (s.isTerminal) {
                            stream << "\033[31;3m" << s.t << "\033[0m";
                        } else {
                            stream << "[" << s.n << ": " << getNonterminalName(s.n) << "]";
                        }
                    }
                }
                stream << '\n';
            }
        }

        return stream.str();
    }

} // namespace cfg
