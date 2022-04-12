#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
#include <sstream>

#include "Grammar.hpp"
#include "Lexer.hpp"

// temporary
#include <iostream>

namespace cfg {

    static const std::tuple<bool, Nonterminal, Rule> FAIL_RULE = { false, UINT_MAX, {} };

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

    void removeUselessRules(Grammar &g) {

        auto step = [] (const Grammar &g) -> Grammar {
            Grammar res = g;

            // mark all rules with a) no productions, or b) only self-replicating productions
                
            std::map<Nonterminal, bool> to_remove;
            for (auto &it : res) {
                to_remove[it.first] = true;
            }

            for (auto &it : res) {
                for (const Rule &r : it.second) {
                    if (r.empty()) { // -> € rule
                        to_remove.at(it.first) = false;
                        break;
                    }

                    for (const Symbol &s : r) {
                        if (s.isTerminal || s.n != it.first) {
                            to_remove.at(it.first) = false;
                            break;
                        }
                    }

                    if (!to_remove.at(it.first)) { break; }
                }
            }

            // mark all rules that do not terminate for removal

            std::map<Nonterminal, bool> termination_map = getTerminateMap(g);
            for (auto &it : termination_map) {
                if (!it.second) {
                    to_remove.at(it.first) = true;
                }
            }

            // remove all entirely self-replicating rules, and the productions which call them, and
            // the rules which become pointless as a result by clearing their productions (recursively)

            bool change;
            do {
                change = false;

                for (auto &it : res) {
                    if (!to_remove.at(it.first)) {
                        // mark nonterminals with no productions for removal
                        if (it.second.empty()) {
                            to_remove.at(it.first) = true;
                            change = true;
                            continue;
                        }

                        // remove productions which contain an marked nonterminal
                        std::vector<Rule> &rules = res.at(it.first);
                        for (unsigned i = 0; i < rules.size(); i++) {
                            if (!rules.at(i).empty() && std::find_if(rules.at(i).begin(), rules.at(i).end(),
                                    [&to_remove] (const Symbol &s) -> bool { return !s.isTerminal && to_remove.at(s.n); }) 
                                    != rules.at(i).end()) {
                                rules.erase(rules.begin() + i);
                                i--;
                                change = true;
                            }
                        }
                    }
                }

            } while (change);

            // remove all marked nonterminals without side-effects
            for (auto it = res.begin(); it != res.end(); ) {
                if (it->first != S && to_remove.at(it->first)) {
                    it = res.erase(it);
                } else {
                    ++it;
                }
            }

            // mark all reachable rules via BFS

            std::map<Nonterminal, bool> reachable;
            for (auto &it : res) {
                reachable[it.first] = false;
            }

            std::deque<Nonterminal> q = { S };
            while (!q.empty()) {
                Nonterminal n = q.front();
                q.pop_front();

                if (reachable.at(n)) {
                    continue;
                }
                reachable.at(n) = true;

                for (const Rule &r : res.at(n)) {
                    for (const Symbol &s : r) {
                        if (!s.isTerminal) {
                            q.push_back(s.n);
                        }
                    }
                }
            }

            // remove all non-reachable rules
            for (auto &it : reachable) {
                if (!it.second) {
                    res.erase(it.first);
                }
            }

            // eliminate duplicate rules
            for (auto &it : res) {
                std::vector<Rule> &rules = it.second;
                for (unsigned i = 0; i < rules.size(); i++) {
                    for (unsigned j = i + 1; j < rules.size(); j++) {
                        if (rules.at(i) == rules.at(j)) {
                            rules.erase(rules.begin() + j);
                        }
                    }
                }
            }

            // eliminate rules with identical production sets
            for (auto it = res.begin(); it != res.end(); ++it) {
                for (auto next = std::next(it); next != res.end(); ) {
                    if (it->second == next->second && next->first != S) {
                        Nonterminal n = it->first, m = next->first;
                        next = res.erase(next);
                        for (auto &p : res) {
                            for (Rule &r : p.second) {
                                for (Symbol &s : r) {
                                    if (!s.isTerminal && s.n == m) {
                                        s.n = n;
                                    }
                                }
                            }
                        }
                        change = true;
                    } else {
                        ++next;
                    }
                }
            }

            return res;
        };

        do {
            Grammar _g = step(g);
            if (_g == g) {
                break;
            }
            g = _g;
        } while (true);
    }

    Nonterminal GrammarManager::addNonterminal(const std::string &name) {
        if (nonterminal_index_map.find(name) != nonterminal_index_map.end()) {
            return nonterminal_index_map.at(name);
        } else {
            nonterminal_index_map[name] = nonterminal_maxindex;
            return nonterminal_maxindex++;
        }
    }

    std::vector<std::tuple<bool, Nonterminal, Rule>> GrammarManager::parseProductionRule(const std::string &line) {        
        std::vector<Token> tokens = lex(line);
        std::size_t index = 0;

        auto inBounds = [&] () -> bool  { return index < tokens.size(); };
        auto peek     = [&] () -> Token { return tokens.at(index); };
        auto advance  = [&] () -> Token { return tokens.at(index++); };
        auto expect   = [&] (Token::Type type) -> Token { 
            if (peek().type == type) { return advance(); }
            throw std::runtime_error("");
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
            Nonterminal rule_index = addNonterminal(nonterminal_token.text);

            bool preferred = select(2, Token::Type::ARROW, Token::Type::PREFER_ARROW).type == Token::Type::PREFER_ARROW;

            std::vector<std::tuple<bool, Nonterminal, Rule>> res;

            while (inBounds()) {
                if (peek().type == Token::Type::EPSILON) {
                    advance();
                    res.push_back({ preferred, rule_index, {} });
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

                res.push_back({ preferred, rule_index, rule_production });
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
        return "";
    }

    void GrammarManager::reset() {
        g.clear();
        nonterminal_index_map[START_SYMBOL] = 0;
        nonterminal_maxindex = 1;

        randgen_preferred = RANDGEN_START;
    }

    GrammarManager::GrammarManager() {
        reset();
    }

    GrammarManager::GrammarManager(const Grammar &g) {
        reset();
        this->g = g;

        if (checkGrammar().first) {
            findTerminatingPath();
        }
    }

    GrammarManager::GrammarManager(const GrammarManager &other) {
        randgen_preferred = other.randgen_preferred;

        g = other.g;
        preferred_rules = other.preferred_rules;

        terminating_paths = other.terminating_paths;

        nonterminal_index_map = other.nonterminal_index_map;
        nonterminal_maxindex = other.nonterminal_maxindex;
    }

    void GrammarManager::parse(const std::vector<std::string> &lines) {
        reset();

        unsigned line_number = 1;
        for (std::string line : lines) {
            if (line.find("!!") == 0) {
                try {
                    randgen_preferred = std::stod(line.substr(2));
                } catch (std::runtime_error &e) {}
                continue;
            } else if (containsNoCode(line)) {
                continue;
            }

            auto newrules = parseProductionRule(line);
            
            for (const auto &r : newrules) {
                if (r == FAIL_RULE) {
                    std::cerr << "Error in Line " << line_number << '\n';
                } else {
                    g[std::get<1>(r)].push_back(std::get<2>(r));
                    if (std::get<0>(r)) {
                        preferred_rules[std::get<1>(r)].push_back(std::get<2>(r));
                    }
                }
            }

            line_number++;
        }

        findTerminatingPath();
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

    void GrammarManager::findTerminatingPath() {
        std::map<Nonterminal, bool> called, known;

        if (!terminates()) {
            throw std::runtime_error("Non-terminating Grammar.");
        }

        for (const auto &it : g) {
            called[it.first] = false;
            known[it.first] = false;
            terminating_paths[it.first] = UINT_MAX;
        }

        std::function<unsigned(Nonterminal)> intermediate = [&] (Nonterminal n) -> unsigned {
            if (called.at(n)) { return UINT_MAX; }
            if (known.at(n)) { return terminating_paths.at(n); }

            called.at(n) = true;

            unsigned rule_index = 0;
            for (const Rule &r : g.at(n)) {
                bool success = true;
                for (const Symbol &s : r) {
                    if (!s.isTerminal && intermediate(s.n) == UINT_MAX) {
                        success = false;
                        break;
                    }
                }
                if (success) {
                    terminating_paths.at(n) = rule_index;
                    break;
                }
                rule_index++;
            }

            known.at(n) = true;
            called.at(n) = false;

            return terminating_paths.at(n);
        };

        // do we need two passes?
        for (const auto &u : g) {
            intermediate(u.first);
        }

        /*terminates();
        for (const auto &u : g) {
            if (!termination_map.at(u.first)) {
                std::cerr << "Nonterminal " << u.first << " doesn't terminate.\n";
            }
        }*/
    }

    std::string GrammarManager::debugInfo() {
        std::stringstream stream;

        terminates();

        for (auto it = g.begin(); it != g.end(); it++) {
            for (const Rule &r : it->second) {
                stream << "[" << it->first << ": " << getNonterminalName(it->first) << "] -> ";
                if (r.empty()) {
                    stream << "€";
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
            stream << " ~~> " << terminating_paths.at(it->first) << "; terminates? " <<
                    (termination_map.at(it->first) ? "Y" : "N") << "\n";
        }

        return stream.str();
    }

    std::string GrammarManager::randGen(Nonterminal n, unsigned recursionDepth) {
        if (g.find(n) == g.end()) {
            throw std::runtime_error("Error: Nonterminal " + std::to_string(n) + " doesn't exist.");
        } else if (g.at(n).empty()) {
            throw std::runtime_error("Error: Nonterminal " + std::to_string(n) + " has no productions.");
        }

        std::stringstream stream;
        const std::vector<Rule> &rules = g.at(n);

        bool selected = false;
        Rule r;

        // discriminate in favor of terminal rules in case of too large recursion depth
        if (recursionDepth > MAX_RECURSION) {
            r = rules.at(terminating_paths.at(n));
            selected = true;
        }

        if (!selected && static_cast<double>(rand()) / RAND_MAX <= randgen_preferred) {
            if (preferred_rules.find(n) != preferred_rules.end() && !preferred_rules.at(n).empty()) {
                const std::vector<Rule> &p_rules = preferred_rules.at(n);
                r = p_rules.at(rand() % p_rules.size());
                selected = true;
            }
        }

        if (!selected) {
            r = rules.at(rand() % rules.size());
        }

        for (const Symbol &s : r) {
            if (s.isTerminal) {
                stream << s.t;
            } else {
                stream << randGen(s.n, recursionDepth + 1);
            }
        }

        return stream.str();
    }

} // namespace cfg