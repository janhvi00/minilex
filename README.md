# MiniLex

A small lexical analyzer (Lex-style) built from regular expressions.

- `minilex.cpp` - the C++ command-line tool. Builds an NFA from regex rules and tokenizes input using longest match.
- `index.html` - the same lexer ported to JavaScript, as a single static web page (no build step, no dependencies).

## Run the C++ version

```
g++ -std=c++17 minilex.cpp -o minilex
./minilex                    # built-in demo
./minilex rules.txt input.txt
```

Rules file: one rule per line, `<regex> <TOKEN_NAME>`. Use `SKIP` to ignore a match. Lines starting with `#` are comments. The earlier rule wins when two rules match the same length.

Supported regex: literals, `.`, `[abc]`, `[a-z]`, `[^abc]`, `( )`, `|`, `*`, `+`, `?`, and escapes like `\n \t \\`.

## Run the web version

Open `index.html` in a browser, or host the folder on GitHub Pages or Vercel.
