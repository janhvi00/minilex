// minilex.cpp  -  g++ -std=c++17 minilex.cpp -o minilex
// Usage: ./minilex [rules.txt] [input.txt]   (no args = built-in demo)
//
// Rules file format (one per line):   <regex>   <TOKEN_NAME>
//   - use SKIP as the token name to ignore a match (whitespace, comments)
//   - lines starting with # are comments
//   - regex supports: literals  .  [abc]  [a-z]  [^abc]  ( )  |  *  +  ?  \n \t \\ etc.
//   - to match a space use [ ] or \x20 is NOT supported, so use [ ]
//   - earlier rule wins when two rules match the same length (put keywords first)

#include <bits/stdc++.h>
using namespace std;

// ---------------------------- NFA ----------------------------
struct Edge {
    int to;
    bool eps;
    bitset<256> set;
};

struct NFA {
    vector<vector<Edge>> g;
    int newState() { g.emplace_back(); return (int)g.size() - 1; }
    void addEps(int a, int b) { g[a].push_back({b, true, {}}); }
    void addSet(int a, int b, const bitset<256>& s) { g[a].push_back({b, false, s}); }
};

struct Frag { int s, e; };

// ------------------------ Regex Parser -----------------------
class RegexParser {
    const string& p;
    size_t i = 0;
    NFA& nfa;

    static char esc(char c) {
        switch (c) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case '0': return '\0';
            default:  return c;
        }
    }

    bool more() const { return i < p.size(); }

    Frag fromSet(const bitset<256>& s) {
        int a = nfa.newState(), b = nfa.newState();
        nfa.addSet(a, b, s);
        return {a, b};
    }

    Frag parseAlt() {
        Frag left = parseConcat();
        while (more() && p[i] == '|') {
            i++;
            Frag right = parseConcat();
            int s = nfa.newState(), e = nfa.newState();
            nfa.addEps(s, left.s);  nfa.addEps(s, right.s);
            nfa.addEps(left.e, e);  nfa.addEps(right.e, e);
            left = {s, e};
        }
        return left;
    }

    Frag parseConcat() {
        if (!more() || p[i] == '|' || p[i] == ')') {      // empty branch
            int s = nfa.newState(), e = nfa.newState();
            nfa.addEps(s, e);
            return {s, e};
        }
        Frag f = parsePostfix();
        while (more() && p[i] != '|' && p[i] != ')') {
            Frag n = parsePostfix();
            nfa.addEps(f.e, n.s);
            f.e = n.e;
        }
        return f;
    }

    Frag parsePostfix() {
        Frag f = parseAtom();
        while (more() && (p[i] == '*' || p[i] == '+' || p[i] == '?')) {
            char op = p[i++];
            int s = nfa.newState(), e = nfa.newState();
            nfa.addEps(s, f.s);
            nfa.addEps(f.e, e);
            if (op == '*' || op == '+') nfa.addEps(f.e, f.s);
            if (op == '*' || op == '?') nfa.addEps(s, e);
            f = {s, e};
        }
        return f;
    }

    Frag parseAtom() {
        char c = p[i++];
        if (c == '(') {
            Frag f = parseAlt();
            if (!more() || p[i] != ')') throw runtime_error("missing ')' in regex: " + p);
            i++;
            return f;
        }
        if (c == '[') return parseClass();
        bitset<256> s;
        if (c == '.') { s.set(); s.reset('\n'); }
        else if (c == '\\') {
            if (!more()) throw runtime_error("dangling '\\' in regex: " + p);
            s.set((unsigned char)esc(p[i++]));
        }
        else if (c == '*' || c == '+' || c == '?')
            throw runtime_error(string("nothing to repeat before '") + c + "' in: " + p);
        else s.set((unsigned char)c);
        return fromSet(s);
    }

    Frag parseClass() {
        bitset<256> s;
        bool neg = false;
        if (more() && p[i] == '^') { neg = true; i++; }
        bool closed = false;
        while (more()) {
            char c = p[i++];
            if (c == ']') { closed = true; break; }
            if (c == '\\' && more()) c = esc(p[i++]);
            if (more() && p[i] == '-' && i + 1 < p.size() && p[i + 1] != ']') {
                i++;
                char hi = p[i++];
                if (hi == '\\' && more()) hi = esc(p[i++]);
                if ((unsigned char)hi < (unsigned char)c)
                    throw runtime_error("bad range in regex: " + p);
                for (int x = (unsigned char)c; x <= (unsigned char)hi; x++) s.set(x);
            } else s.set((unsigned char)c);
        }
        if (!closed) throw runtime_error("missing ']' in regex: " + p);
        if (neg) s.flip();
        return fromSet(s);
    }

public:
    RegexParser(const string& pattern, NFA& n) : p(pattern), nfa(n) {}
    Frag parse() {
        Frag f = parseAlt();
        if (more()) throw runtime_error("unmatched ')' in regex: " + p);
        return f;
    }
};

// --------------------------- Lexer ---------------------------
struct Rule { string pattern, name; };

struct Token {
    string type, lexeme;
    int line, col;
};

class Lexer {
    NFA nfa;
    int start;
    vector<int> acceptRule;     // state -> rule index (or -1)
    vector<Rule> rules;

    vector<int> closure(const vector<int>& in) const {
        vector<char> mark(nfa.g.size(), 0);
        vector<int> out, st;
        for (int s : in) if (!mark[s]) { mark[s] = 1; st.push_back(s); out.push_back(s); }
        while (!st.empty()) {
            int u = st.back(); st.pop_back();
            for (const Edge& e : nfa.g[u])
                if (e.eps && !mark[e.to]) { mark[e.to] = 1; st.push_back(e.to); out.push_back(e.to); }
        }
        return out;
    }

    vector<int> step(const vector<int>& cur, unsigned char c) const {
        vector<int> nxt;
        for (int u : cur)
            for (const Edge& e : nfa.g[u])
                if (!e.eps && e.set[c]) nxt.push_back(e.to);
        return nxt;
    }

public:
    void build(const vector<Rule>& r) {
        rules = r;
        nfa = NFA();
        start = nfa.newState();
        vector<pair<int,int>> accepts;
        for (size_t k = 0; k < rules.size(); k++) {
            RegexParser rp(rules[k].pattern, nfa);
            Frag f = rp.parse();
            nfa.addEps(start, f.s);
            accepts.push_back({f.e, (int)k});
        }
        acceptRule.assign(nfa.g.size(), -1);
        for (auto& [st, k] : accepts) acceptRule[st] = k;
    }

    vector<Token> tokenize(const string& text, vector<string>& errors) const {
        vector<Token> out;
        size_t pos = 0;
        int line = 1, col = 1;
        while (pos < text.size()) {
            vector<int> cur = closure({start});
            int lastRule = -1;
            size_t lastEnd = pos;
            for (size_t i = pos; i < text.size() && !cur.empty(); i++) {
                cur = closure(step(cur, (unsigned char)text[i]));
                int best = -1;
                for (int s : cur) {
                    int r = acceptRule[s];
                    if (r >= 0 && (best < 0 || r < best)) best = r;
                }
                if (best >= 0) { lastRule = best; lastEnd = i + 1; }
            }
            if (lastRule < 0) {
                errors.push_back("Lexical error at " + to_string(line) + ":" + to_string(col) +
                                 " -> unexpected character '" + string(1, text[pos]) + "'");
                if (text[pos] == '\n') { line++; col = 1; } else col++;
                pos++;
                continue;
            }
            string lex = text.substr(pos, lastEnd - pos);
            if (rules[lastRule].name != "SKIP")
                out.push_back({rules[lastRule].name, lex, line, col});
            for (char ch : lex) {
                if (ch == '\n') { line++; col = 1; } else col++;
            }
            pos = lastEnd;
        }
        return out;
    }
};

// --------------------------- Helpers -------------------------
static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

vector<Rule> parseRules(istream& in) {
    vector<Rule> rules;
    string line;
    int ln = 0;
    while (getline(in, line)) {
        ln++;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t sp = line.find_last_of(" \t");
        if (sp == string::npos)
            throw runtime_error("rules line " + to_string(ln) + ": expected '<regex> <TOKEN>'");
        string pat = trim(line.substr(0, sp));
        string name = trim(line.substr(sp + 1));
        if (pat.empty() || name.empty())
            throw runtime_error("rules line " + to_string(ln) + ": empty pattern or token name");
        rules.push_back({pat, name});
    }
    return rules;
}

static const char* DEFAULT_RULES = R"RULES(
# keywords first so they beat IDENTIFIER on equal length
if|else|while|for|return|int|float|char|void|double   KEYWORD
[a-zA-Z_][a-zA-Z0-9_]*                                 IDENTIFIER
[0-9]+\.[0-9]+                                         FLOAT_LITERAL
[0-9]+                                                 INT_LITERAL
"([^"\\]|\\.)*"                                        STRING_LITERAL
//[^\n]*                                               SKIP
==|!=|<=|>=|&&|\|\||\+\+|--|[-+*/%=<>!&|]              OPERATOR
[(){};,\[\]]                                           PUNCTUATION
[ \t\r\n]+                                             SKIP
)RULES";

static const char* DEFAULT_INPUT = R"SRC(int main() {
    float x = 3.14;   // pi-ish
    if (x >= 3 && x != 4) {
        printf("hello\n");
    }
    int count = 10 @ 2;
    return 0;
}
)SRC";

// ---------------------------- main ---------------------------
int main(int argc, char* argv[]) {
    try {
        vector<Rule> rules;
        string text;

        if (argc >= 2) {
            ifstream rf(argv[1]);
            if (!rf) { cerr << "Cannot open rules file: " << argv[1] << "\n"; return 1; }
            rules = parseRules(rf);
        } else {
            istringstream rs(DEFAULT_RULES);
            rules = parseRules(rs);
        }

        if (argc >= 3) {
            ifstream inf(argv[2]);
            if (!inf) { cerr << "Cannot open input file: " << argv[2] << "\n"; return 1; }
            stringstream ss; ss << inf.rdbuf(); text = ss.str();
        } else {
            text = DEFAULT_INPUT;
        }

        Lexer lx;
        lx.build(rules);

        vector<string> errors;
        vector<Token> toks = lx.tokenize(text, errors);

        cout << left << setw(8) << "LINE:COL" << setw(18) << "TOKEN" << "LEXEME\n";
        cout << string(50, '-') << "\n";
        map<string,int> counts;
        for (auto& t : toks) {
            string pos = to_string(t.line) + ":" + to_string(t.col);
            string lexShown = t.lexeme;
            for (char& ch : lexShown) if (ch == '\n') ch = ' ';
            cout << left << setw(8) << pos << setw(18) << t.type << lexShown << "\n";
            counts[t.type]++;
        }

        cout << "\n--- Summary ---\n";
        for (auto& [k, v] : counts) cout << setw(18) << k << v << "\n";
        cout << "Total tokens: " << toks.size() << "\n";

        if (!errors.empty()) {
            cout << "\n--- Errors ---\n";
            for (auto& e : errors) cout << e << "\n";
        }
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}