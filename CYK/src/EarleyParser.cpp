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
        struct Index {
            unsigned col, row;
            bool operator == (const Index &other) {
                return col == other.col && row == other.row;
            }
        } previous;

        inline bool operator == (const EarleyItem &i) const {
            return start == i.start && from == i.from && pre == i.pre && post == i.post; // ignore 'previous' item
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
    static void removeDuplicates(Table &DP, const unsigned col);

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
            DP.at(col).push_back(EarleyItem{ col, entry.post.front().n, {}, r, EarleyItem::Index{ col, row } });
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
            DP.at(col + 1).push_back(EarleyItem{ entry.start, entry.from, entry.pre + entry.post.front(), Symbols(entry.post.begin() + 1, entry.post.end()), EarleyItem::Index{ col, row } });
            return true;
        }

        return false;
    }

    static void scanExtra(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row, const std::map<Nonterminal, bool> &nullable_map) {
        const EarleyItem entry = DP.at(col).at(row);

        for (auto it = entry.post.begin(); it != entry.post.end(); ++it) {
            if (it->isTerminal || !nullable_map.at(it->n)) { break; }
            DP.at(col).push_back(EarleyItem{ entry.start, entry.from, entry.pre + Symbols(entry.post.begin(), it + 1), Symbols(it + 1, entry.post.end()), EarleyItem::Index{ col, row } });
        }
    }

    static std::string earleyName(const std::vector<Symbol> &pre, const std::vector<Symbol> &post) {
        static const auto toString = [] (Symbol s) -> std::string {
            if (s.isTerminal) {
                return std::string(1, s.t);
            } else {
                return "[" + std::to_string(s.n) + "]";
            }
        };
        
        std::stringstream stream;
        for (unsigned i = 0; i < pre.size(); i++) {
            stream << toString(pre.at(i));
        }
        stream << "*";
        for (unsigned i = 0; i < post.size(); i++) {
            stream << toString(post.at(i));
        }

        return stream.str();
    }

    static bool complete(const Grammar &g, const String &input, Table &DP, unsigned col, unsigned row) {
        const EarleyItem entry = DP.at(col).at(row);

        if (!entry.post.empty()) {
            return false;
        }

        for (unsigned i = 0; i < DP.at(entry.start).size(); i++) {
            const EarleyItem &before_entry = DP.at(entry.start).at(i);
            if (!before_entry.post.empty() && before_entry.post.front() == Symbol{ false, { .n = entry.from } }) {
                DP.at(col).push_back({ before_entry.start, before_entry.from, before_entry.pre + before_entry.post.front(), Symbols(before_entry.post.begin() + 1, before_entry.post.end()), EarleyItem::Index{ col, row } });
            } 
        }

        return true;
    }

    static void removeDuplicates(Table &DP, const unsigned col) {
        static const auto remove = [&DP] (unsigned col, unsigned i, unsigned j) -> void {
            for (unsigned k = j + 1; k < DP.at(col).size(); k++) {
                if (DP.at(col).at(k).previous == EarleyItem::Index{ col, j }) {
                    DP.at(col).at(k).previous.row = i;
                }
            }
            if (col + 1 >= DP.size()) { return; }
            for (unsigned k = 0; k < DP.at(col + 1).size(); k++) {
                if (DP.at(col + 1).at(k).previous == EarleyItem::Index{ col, j }) {
                    DP.at(col + 1).at(k).previous = EarleyItem::Index{ col, i };
                }
            }

            DP.at(col).erase(DP.at(col).begin() + j);
        };

        // remove duplicate predicts in current column
        for (unsigned i = 0; i < DP.at(col).size(); i++) {
            for (unsigned j = i + 1; j < DP.at(col).size(); j++) {
                if (DP.at(col).at(i) == DP.at(col).at(j)) {
                    remove(col, i, j);
                }
            }
        }

        // remove duplicate scans in next column
        if (col >= DP.size() - 1) { return; }
        for (unsigned i = 0; i < DP.at(col + 1).size(); i++) {
            for (unsigned j = i + 1; j < DP.at(col + 1).size(); j++) {
                if (DP.at(col + 1).at(i) == DP.at(col + 1).at(j)) {
                    remove(col + 1, i, j);
                }
            }
        }
    }

    static std::string printTable(const Table &DP, const std::vector<Terminal> &input) {
        static constexpr unsigned LEN = 20;

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
                    stream << DP.at(col).at(row).start << ":" <<
                            (DP.at(col).at(row).from == ROOT ? "^" : std::to_string(DP.at(col).at(row).from)) << "->" <<
                            std::setw(LEN - 5) << earleyName(DP.at(col).at(row).pre, DP.at(col).at(row).post);
                } else {
                    stream << std::setw(LEN) << ' ';
                }
                stream << "|";
            }
            stream << "\n";
        }

        return stream.str();
    }

    EarleyParser::EarleyParser() {}

    void EarleyParser::initGrammar(const std::string &filename) {
        gm.parseFromFile(filename);
        if (!gm.checkGrammar().first) {
            throw std::runtime_error("Bad Grammar");
        }
    }

    bool EarleyParser::parseInput(const String &input) {
        return parseTree(input).first;
    }

    std::pair<bool, ParseTree> EarleyParser::parseTree(const String &input) {
        std::map<Nonterminal, bool> nullables = getNullableRules(gm.g);
        nullables[ROOT] = nullables.at(S);

        Table DP;

        DP.resize(input.size() + 1);
        DP.at(0).push_back(EarleyItem{ 0, ROOT, Symbols{}, Symbols{ { false, { .n = S } } }, { UINT_MAX, UINT_MAX } });

        for (unsigned i = 0; i < DP.size(); i++) {
            for (unsigned j = 0; j < DP.at(i).size(); j++) {
                scan(gm.g, input, DP, i, j);
                scanExtra(gm.g, input, DP, i, j, nullables);
                complete(gm.g, input, DP, i, j);
                predict(gm.g, input, DP, i, j);
            }
            removeDuplicates(DP, i);
        }

        // std::cout << printTable(DP, input) << "\n\n";

        auto it = std::find_if(DP.back().begin(), DP.back().end(),
            [] (EarleyItem i) -> bool { return i.from == ROOT && i.post.empty(); });
        if (it == DP.back().end()) {
            return { false, FAILED_PARSE };
        }

        EarleyItem finalParse = *it;
        std::vector<std::pair<EarleyItem, unsigned /*end*/>> parsePath = { std::make_pair(finalParse, DP.size() - 1) };

        while (parsePath.back().first.previous.col != UINT_MAX) {
            parsePath.push_back(std::make_pair(DP.at(parsePath.back().first.previous.col).at(parsePath.back().first.previous.row), parsePath.back().first.previous.col));
        }

        // std::erase_if(parsePath, [] (std::pair<EarleyItem, unsigned> item) -> bool { return !item.first.post.empty(); });

        struct SimpleItem {
            Nonterminal n;
            Rule production;
        };

        // std::vector<std::vector<std::vector<SimpleItem>>> DP;



        return { true, FAILED_PARSE };
    }

} // namespace cfg