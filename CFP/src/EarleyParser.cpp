#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <vector>

#include "EarleyParser.hpp"

// temporary
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace cfg {

    template<typename T>
    static std::vector<T> operator + (const std::vector<T> &left, const T &right) {
        std::vector<T> result = left;
        result.push_back(right);
        return result;
    }

    template<typename T>
    static std::vector<T> operator + (const std::vector<T> &left, const std::vector<T> &right) {
        std::vector<T> result = left;
        result.insert(result.end(), right.begin(), right.end());
        return result;
    }

    struct EarleyItem {
        unsigned start;
        Nonterminal from;
        Symbols pre, post;

        enum class DerivationType { ROOT, SCAN, NULLABLE_SCAN, PREDICT, COMPLETE } type;
        std::pair<std::pair<unsigned, unsigned>, std::pair<unsigned, unsigned>> backpointer;

        inline bool operator == (const EarleyItem &i) const {
            return start == i.start && from == i.from && pre == i.pre && post == i.post; // ignore antecedent pointers
        }

        inline operator std::string() const {
            std::stringstream stream;

            stream << "[ @" << start << "-? : <";
            if (from == ROOT) {
                stream << "^";
            } else {
                stream << from;
            }
            stream << "> -> ";

            for (const Symbol &s : pre) {
                if (s.isTerminal) {
                    stream << s.t;
                } else {
                    stream << "<" << s.n << ">";
                }
            }
            stream << " * ";
            for (const Symbol &s : post) {
                if (s.isTerminal) {
                    stream << s.t;
                } else {
                    stream << "<" << s.n << ">";
                }
            }
            stream << " : ";
            if (type != DerivationType::COMPLETE) {
                stream << "(" << backpointer.first.first << "/" << backpointer.first.second << ")";
            } else {
                stream << "{(" << backpointer.first.first << "/" << backpointer.first.second << ")," <<
                        backpointer.second.first << "/" << backpointer.second.second << ")}";
            }
            stream << " ]";
            return stream.str();
        }
    };

    static std::map<Nonterminal, bool> getNullableRules(const Grammar &g) {
        std::map<Nonterminal, bool> res, called, known;
        for (const auto &u : g) {
            res[u.first] = false;
            called[u.first] = false;
            known[u.first] = false;
        }

        std::function<bool(Nonterminal)> step = [&] (Nonterminal n) -> bool {
            if (called.at(n)) { return false; }
            if (known.at(n)) { return res.at(n); }

            called.at(n) = true;

            for (const Rule &r : g.at(n)) {
                if (std::find_if(r.begin(), r.end(), [&] (Symbol s) -> bool { return s.isTerminal || !step(s.n); }) == r.end()) {
                    res.at(n) = true;
                    break;
                }
            }

            called.at(n) = false;
            known.at(n) = true;
            return res.at(n);
        };

        for (const auto &u : g) {
            step(u.first);
        }

        return res;
    }

    using Table = std::vector<std::vector<EarleyItem>>;

    static bool predict(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row);
    static bool scan(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row);
    static void scanExtra(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row, const std::map<Nonterminal, bool> &nullable_map);
    static bool complete(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row);
    static void removeDuplicates(Table &DP, const unsigned col) {}

    static bool predict(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row) {
        const EarleyItem entry = DP.at(col).at(row);

        // do not predict an item that doesn't start with a nonterminal
        if (entry.post.empty() || entry.post.front().isTerminal) {
            return false;
        }

        // do not predict an item that has already been predicted
        for (unsigned i = 0; i < row; i++) {
            if (!DP.at(col).at(i).post.empty() && DP.at(col).at(i).post.front() == entry.post.front()) {
                return false;
            }
        }

        // predict all production rules from the item
        for (const Rule &r : g.at(entry.post.front().n)) {
            EarleyItem next_item = { col, entry.post.front().n, {}, r, EarleyItem::DerivationType::PREDICT, { { col, row }, { 0, 0 } } };
            if (!contains(DP.at(col), next_item)) {
                DP.at(col).push_back(next_item);
            }
        }
        return true;
    }

    static bool scan(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row) {
        const EarleyItem &entry = DP.at(col).at(row);

        // do not scan at the end of the input, a fully parsed item or a nonterminal
        if (col == input.size() || entry.post.empty() || !entry.post.front().isTerminal) {
            return false;
        }

        if (input.at(col) == entry.post.front().t) {
            EarleyItem next_item = { entry.start, entry.from, entry.pre + entry.post.front(), Symbols(entry.post.begin() + 1, entry.post.end()), EarleyItem::DerivationType::SCAN, { { col, row }, { 0, 0 } } };
            if (!contains(DP.at(col + 1), next_item)) {
                DP.at(col + 1).push_back(next_item);
            }
            return true;
        }

        return false;
    }

    static void scanExtra(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row, const std::map<Nonterminal, bool> &nullable_map) {
        const EarleyItem entry = DP.at(col).at(row);

        if (!entry.post.empty() && !entry.post.front().isTerminal && nullable_map.at(entry.post.front().n)) {
            EarleyItem next_item = { entry.start, entry.from, entry.pre + entry.post.front(), Symbols(entry.post.begin() + 1, entry.post.end()), EarleyItem::DerivationType::NULLABLE_SCAN, { { col, row }, { 0, 0 } } };
            if (!contains(DP.at(col), next_item)) {
                DP.at(col).push_back(next_item);
            }
        }
    }

    static bool complete(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row) {
        const EarleyItem entry = DP.at(col).at(row);

        if (!entry.post.empty()) {
            return false;
        }

        for (unsigned i = 0; i < DP.at(entry.start).size(); i++) {
            const EarleyItem &before_entry = DP.at(entry.start).at(i);
            if (!before_entry.post.empty() && before_entry.post.front() == Symbol{ false, { .n = entry.from } }) {
                EarleyItem next_item = { before_entry.start, before_entry.from, before_entry.pre + before_entry.post.front(), Symbols(before_entry.post.begin() + 1, before_entry.post.end()), EarleyItem::DerivationType::COMPLETE, { { entry.start, i }, { col, row } } };
                if (!contains(DP.at(col), next_item)) {
                    DP.at(col).push_back(next_item);
                }
            } 
        }

        return true;
    }

    static std::string printTable(const Table &DP, const std::vector<Terminal> &input) {
        static constexpr unsigned LEN = 70;

        std::stringstream stream;

        std::size_t max = 0;
        for (const auto &u : DP) {
            max = std::max(max, u.size());
        }

        stream << "|";
        for (unsigned col = 0; col < DP.size(); col++) {
            stream << std::setw(LEN) << (col == DP.size() - 1 ? ' ' : input.at(col)) << "|";
        }
        stream << "\n\n";
        for (unsigned row = 0; row < max; row++) {
            stream << "|";
            for (unsigned col = 0; col < DP.size(); col++) {
                if (row < DP.at(col).size()) {
                    stream << std::setw(LEN) << static_cast<std::string>(DP.at(col).at(row));
                } else {
                    stream << std::setw(LEN) << ' ';
                }
                stream << "|";
            }
            stream << "\n";
        }

        return stream.str();
    }

    // CONTRACT: This function assumes that <n> is nullable, otherwise unpredictable behavior
    static ProductionTree getNullTree(const Grammar &g, const Nonterminal n) {
        static std::map<Nonterminal, bool> called, known;
        static std::map<Nonterminal, std::pair<bool, ProductionTree>> nullTrees;

        for (const auto &it : g) {
            called[it.first] = false;
            known[it.first] = false;
            nullTrees[it.first] = { false, FAILED_PARSE };
        }

        static const std::function<std::pair<bool, ProductionTree>(Nonterminal)> step = [&] (Nonterminal n) -> std::pair<bool, ProductionTree> {
            if (known.at(n)) { return nullTrees.at(n); }
            if (called.at(n)) { return { false, FAILED_PARSE }; }

            called.at(n) = true;

            for (const Rule &r : g.at(n)) {
                std::pair<bool, ProductionTree> tmpres = { true, { n, r, {} } };
                for (const Symbol &s : r) {
                    if (s.isTerminal) {
                        tmpres.first = false;
                        break;
                    } else {
                        std::pair<bool, ProductionTree> subtree = step(s.n);
                        if (!subtree.first) {
                            tmpres.first = false;
                            break;
                        } else {
                            tmpres.second.subtrees.push_back(subtree.second);
                        }
                    }
                }
                if (tmpres.first) {
                    nullTrees.at(n) = tmpres;
                    break;
                }
            }

            known.at(n) = true;
            called.at(n) = false;
            return nullTrees.at(n);
        };

        return step(n).second;
    }

    static ProductionTree backtrack(const Grammar &g, const Table &DP, const EarleyItem &target) {
        ProductionTree result{ target.from, target.pre + target.post, std::vector<ProductionTree>{} };
        const EarleyItem *current_item = &target;

        while (!current_item->pre.empty()) {
            switch (current_item->type) {
                case EarleyItem::DerivationType::SCAN:
                    current_item = &DP.at(current_item->backpointer.first.first).at(current_item->backpointer.first.second);
                    break;
                case EarleyItem::DerivationType::NULLABLE_SCAN:
                    result.subtrees.push_back(getNullTree(g, current_item->pre.back().n));
                    current_item = &DP.at(current_item->backpointer.first.first).at(current_item->backpointer.first.second);
                    break;
                case EarleyItem::DerivationType::COMPLETE:
                    result.subtrees.push_back(backtrack(g, DP, DP.at(current_item->backpointer.second.first).at(current_item->backpointer.second.second)));
                    current_item = &DP.at(current_item->backpointer.first.first).at(current_item->backpointer.first.second);
                    break;
                case EarleyItem::DerivationType::ROOT:    // never reached
                case EarleyItem::DerivationType::PREDICT: // never reached
                    break;
            }
        }

        std::reverse(result.subtrees.begin(), result.subtrees.end());

        return result;
    }

    EarleyParser::EarleyParser() {}

    void EarleyParser::initGrammar(const std::string &filename) {
        gm.parseFromFile(filename);
    }

    bool EarleyParser::parseInput(const String &input) {
        return parseTree(input).first;
    }

    std::pair<bool, ProductionTree> EarleyParser::parseTree(const String &input) {
        std::map<Nonterminal, bool> nullables = getNullableRules(gm.g);
        nullables[ROOT] = nullables.at(S);

        Table DP;

        DP.resize(input.size() + 1);
        DP.at(0).push_back(EarleyItem{ 0, ROOT, Symbols{}, Symbols{ { false, { .n = S } } }, EarleyItem::DerivationType::ROOT, { { 0, 0 }, { 0, 0 } } });

        for (unsigned i = 0; i < DP.size(); i++) {
            for (unsigned j = 0; j < DP.at(i).size(); j++) {
                scan(gm.g, input, DP, i, j);
                scanExtra(gm.g, input, DP, i, j, nullables);
                complete(gm.g, input, DP, i, j);
                predict(gm.g, input, DP, i, j);
            }
        }

        std::cout << gm.debugInfo() << "\n\n" << printTable(DP, input) << "\n\n";

        auto it = std::find_if(DP.back().begin(), DP.back().end(),
            [] (EarleyItem i) -> bool { return i.from == ROOT && i.post.empty(); });
        if (it == DP.back().end()) {
            return { false, FAILED_PARSE };
        }

        ProductionTree tree = backtrack(gm.g, DP, *it);
        std::cout << gm.printTree(tree) << "\n\n";
        tree = gm.refineTree(tree);
        std::cout << gm.printTree(tree) << "\n\n";
        return { true, tree };
    }

} // namespace cfg