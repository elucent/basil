#include "lex.h"

namespace basil {
    
    const u32
        TOKEN_NONE = 0, 
        TOKEN_IDENT = 1, 
        TOKEN_STRING = 2, 
        TOKEN_CHAR = 3, 
        TOKEN_NUMBER = 4, 
        TOKEN_SYMBOL = 5,
        TOKEN_LPAREN = 6, 
        TOKEN_RPAREN = 7, 
        TOKEN_LBRACE = 8, 
        TOKEN_RBRACE = 9,
        TOKEN_LBRACK = 10, 
        TOKEN_RBRACK = 11,
        TOKEN_COLON = 12, 
        TOKEN_SEMI = 13, 
        TOKEN_NEWLINE = 14, 
        TOKEN_ASSIGN = 15,
        TOKEN_LAMBDA = 16,
        TOKEN_DOT = 17,
        TOKEN_PLUS = 18,
        TOKEN_MINUS = 19,
        TOKEN_EVAL = 20,
        TOKEN_BOOL = 21,
        TOKEN_REF = 22,
        TOKEN_QUOTE = 23;
    
    const char* TOKEN_NAMES[24] = {
        "none",
        "ident",
        "string",
        "char",
        "number",
        "symbol",
        "left paren",
        "right paren",
        "left brace",
        "right brace",
        "left bracket",
        "right bracket",
        "colon",
        "semicolon",
        "newline",
        "assign",
        "lambda",
        "dot",
        "plus",
        "minus",
        "eval",
        "bool",
        "ref",
        "quote"
    };

    Token::Token(): type(TOKEN_NONE) {
        //
    }

    Token::Token(const ustring& value_in, u32 type_in, 
            u32 line_in, u32 column_in):
        value(value_in), type(type_in), line(line_in), column(column_in) {
        //
    }

    Token::operator bool() const {
        return type != TOKEN_NONE;
    }

    const Token TokenCache::NONE;

    TokenCache::TokenCache(Source* src): _src(src) {
        // 
    }

    void TokenCache::push(const Token& t) {
        tokens.push(t);
    }

    TokenCache::View::View(TokenCache* cache): _cache(cache), i(0) {
        //
    }

    TokenCache::View::View(TokenCache* cache, u32 index): 
        _cache(cache), i(index) {
        //
    }

    const Token& TokenCache::View::read() {
        if (i >= _cache->size()) return TokenCache::NONE;
        return _cache->tokens[i ++];    
    }

    const Token& TokenCache::View::peek() const {
        if (i >= _cache->size()) return TokenCache::NONE;
        return _cache->tokens[i];
    }

    TokenCache& TokenCache::View::cache() {
        return *_cache;
    }

    TokenCache::View::operator bool() const {
        return i < _cache->size();
    }

    const Token* TokenCache::begin() const {
        return tokens.begin();
    }

    const Token* TokenCache::end() const {
        return tokens.end();
    }

    Source* TokenCache::source() {
        return _src;
    }

    TokenCache::View TokenCache::view() {
        return View(this);
    }

    TokenCache::View TokenCache::expand(stream& io) {
        auto v = _src->expand(io);
        println(io, "Source: ", *_src);
        View mv(this, tokens.size());
        while (v.peek()) {
            Token t = scan(v);
            if (t) push(t);
        }

        return mv;
    }

    u32 TokenCache::size() const {
        return tokens.size();
    }

    static bool isDelimiter(Source::View& view) {
        uchar c = view.peek();
        if (c == ':') {
            view.read();
            bool delim = isspace(view.peek());
            view.rewind();
            return delim;
        }
        return !c || isspace(c) 
            || c == '(' || c == ')' || c == '{' || c == '}' 
            || c == ';' || c == ',' || c == '[' 
            || c == ']' || c == '\''|| c == '"' || c == '.';
    }

    static bool isClosingDelimiter(Source::View& view) {
        uchar c = view.peek();
        return !c || isspace(c) 
            || c == ')'|| c == '}' 
            || c == ';' || c == ','
            || c == ']' || c == '.';
    }

    static bool isDelimiterToken(Source::View& view) {
        uchar c = view.peek();
        if (c == ':') {
            view.read();
            bool delim = isspace(view.peek());
            view.rewind();
            return delim;
        }
        return c == '(' || c == ')' || c == '{' || c == '}' 
            || c == ';' || c == ',' || c == '[' 
            || c == ']' || c == '\n'|| c == '.';
    }

    static Token fromType(u32 type, const Source::View& view) {
        return Token("", type, view.line(), view.column());
    }

    static Token fromValue(u32 type, const ustring& val, const Source::View& view) {
        return Token(val, type, view.line(), view.column());
    }

    static Token getDelimiterToken(uchar c, const Source::View& view) {
        if (c == '(') return fromType(TOKEN_LPAREN, view);
        else if (c == ')') return fromType(TOKEN_RPAREN, view);
        else if (c == '{') return fromType(TOKEN_LBRACE, view);
        else if (c == '}') return fromType(TOKEN_RBRACE, view);
        else if (c == '[') return fromType(TOKEN_LBRACK, view);
        else if (c == ']') return fromType(TOKEN_RBRACK, view);
        else if (c == ':') return fromType(TOKEN_COLON, view);
        else if (c == ';') return fromType(TOKEN_SEMI, view);
        else if (c == '\n') return fromType(TOKEN_NEWLINE, view);
        else if (c == '.') return fromType(TOKEN_DOT, view);
        else if (c == ',') return fromValue(TOKEN_IDENT, ",", view);
        else return Token();
    }

    void scanNumberTail(Token& t, Source::View& view) {
        while (!isDelimiter(view)) {
            if (isdigit(view.peek())) t.value += view.read();
            else {
                err(PHASE_LEX, view.source(), view.line(), view.column(),
                    "Unexpected symbol '", view.peek(),
                    "' in numeric literal.");
                break;
            }
        }
    }

    void scanNumberHead(Token& t, Source::View& view) {
        while (!isDelimiter(view) || view.peek() == '.') {
            if (isdigit(view.peek())) t.value += view.read();
            else if (view.peek() == '.') {
                uchar dot = view.read();
                if (isdigit(view.peek())) {
                    return t.value += dot, scanNumberTail(t, view);
                }
                else {
                    view.rewind();
                    break;
                }
            }
            else {
                err(PHASE_LEX, view.source(), view.line(), view.column(),
                    "Unexpected symbol '", view.peek(),
                    "' in numeric literal.");
                break;
            }
        }
    }

    void scanIdentifier(Token& t, Source::View& view);

    void scanEscape(Token& t, Source::View& view) {
        view.read(); // consume backslash
        if (view.peek() == 'n') t.value += '\n', view.read();
        else if (view.peek() == 't') t.value += '\t', view.read();
        else if (view.peek() == 'r') t.value += '\r', view.read();
        else if (view.peek() == '0') t.value += '\0', view.read();
        else if (view.peek() == '\\') t.value += '\\', view.read();
        else if (view.peek() == '\"') t.value += '\"', view.read();
        else if (view.peek() == '\'') t.value += '\'', view.read();
        else {
            err(PHASE_LEX, view.source(), view.line(), view.column(),
                "Invalid escape sequence '\\", view.peek(), "'.");
        }
    }

    void scanString(Token& t, Source::View& view) {
        view.read(); // consume preceding quote
        while (view.peek() != '"') {
            if (!view.peek()) {
                err(PHASE_LEX, view.source(), view.line(), view.column(),
                    "Unexpected end of input in string literal.");
                break;
            }
            else if (view.peek() == '\n') {
                err(PHASE_LEX, view.source(), view.line(), view.column(),
                    "Unexpected end of line in string literal.");
                break;
            }
            else if (view.peek() == '\\') scanEscape(t, view);
            else t.value += view.read();
        }
        view.read(); // consume trailing quote
    }

    void scanChar(Token& t, Source::View& view) {
        view.read(); // consume preceding quote
        if (!view.peek()) {
            err(PHASE_LEX, view.source(), view.line(), view.column(),
                "Unexpected end of input in character literal.");
        }
        else if (view.peek() == '\\') scanEscape(t, view);
        else if (view.peek() == '\n') {
            err(PHASE_LEX, view.source(), view.line(), view.column(),
                "Unexpected end of line in character literal.");
        }
        else t.value += view.read();
        if (view.peek() != '\'') {
            err(PHASE_LEX, view.source(), view.line(), view.column(),
                "Expected closing quote in character literal, ",
                "found unexpected symbol '", view.peek(), "'.");
        }
        view.read(); // consume trailing quote
    }

    void scanDot(Token& t, Source::View& view) {
        t.value += view.read();
        if (view.peek() == '.') scanDot(t, view);
        else {
            if (t.value == ".") t = fromType(TOKEN_DOT, view);
            else t = fromValue(TOKEN_IDENT, t.value, view);
        }
    }

    void scanPrefixColon(Token& t, Source::View& view) {
        if (isDelimiterToken(view)) 
            return t = getDelimiterToken(view.read(), view), void();

        t.value += view.read();

        // special colon cases
        if (view.peek() == ':') {
            view.read();

            // :: is identifier
            if (view.peek() != ':' && isDelimiter(view))
                t.type = TOKEN_IDENT, t.value += ':';
            else view.rewind();
        }
    }

    void scanPrefixOp(Token& t, Source::View& view) {
        t.value += view.read();
        
        // treat as identifier if followed by special symbol
        if (view.peek() == '-' || view.peek() == '+' || view.peek() == '='
            || view.peek() == '>' || view.peek() == '!' 
            || view.peek() == '~' || isClosingDelimiter(view)) {
            t.type = TOKEN_IDENT, scanIdentifier(t, view);
        }
        else if (isspace(view.peek())) t.type = TOKEN_IDENT;
    }

    void scanIdentifier(Token& t, Source::View& view) {
        while (!isDelimiter(view) || 
               (view.peek() == ':' 
                && t.value[t.value.size() - 1] == ':')) {
            if (issym(view.peek())) t.value += view.read();
            else {
                err(PHASE_LEX, view.source(), view.line(), view.column(),
                    "Unexpected symbol '", view.peek(),
                    "' in identifier.");
                break;
            }
        }
        if (t.value[0] == '_') 
            err(PHASE_LEX, view.source(), view.line(), view.column(),
                "Identifiers may not begin with underscores.");
        if (t.value == "->") t.type = TOKEN_LAMBDA, t.value = "";
        if (t.value == "=") t.type = TOKEN_ASSIGN, t.value = "";
        if (t.value == "true" || t.value == "false") t.type = TOKEN_BOOL;
    }

    Token scan(Source::View& view) {
        uchar c = view.peek();
        Token t;
        if (c == '#') {
            // line comments
            while (view.peek() != '\n') view.read();
        }
        else if (c == '.') {
            t = fromType(TOKEN_IDENT, view);
            scanDot(t, view);
        }
        else if (c == '-') {
            t = fromType(TOKEN_MINUS, view);
            scanPrefixOp(t, view);
        }
        else if (c == '+') {
            t = fromType(TOKEN_PLUS, view);
            scanPrefixOp(t, view);
        }
        else if (c == ':') {
            t = fromType(TOKEN_QUOTE, view);
            scanPrefixColon(t, view);
        }
        else if (c == '!') {
            t = fromType(TOKEN_EVAL, view);
            scanPrefixOp(t, view);
        }
        else if (c == '~') {
            t = fromType(TOKEN_REF, view);
            scanPrefixOp(t, view);
        }
        else if (isdigit(c)) {
            t = fromType(TOKEN_NUMBER, view);
            scanNumberHead(t, view);
        }
        else if (isDelimiterToken(view)) {
            t = getDelimiterToken(c, view);
            view.read();
        }
        else if (c == '"') {
            t = fromType(TOKEN_STRING, view);
            scanString(t, view);
        }
        else if (c == '\'') {
            t = fromType(TOKEN_CHAR, view);
            scanChar(t, view);
        }
        else if (issym(c)) {
            t = fromType(TOKEN_IDENT, view);
            scanIdentifier(t, view);
        }
        else if (isspace(c)) view.read();
        else {
            err(PHASE_LEX, view.source(), view.line(), view.column(),
                "Unexpected symbol '", view.peek(), "' in input.");
            view.read();
        }

        return t;
    }

    TokenCache lex(Source& src) {
        auto view = src.view();
        TokenCache cache(&src);
        while (view.peek()) {
            Token t = scan(view);
            if (t) cache.push(t);
        }

        if (countErrors() > 0) {
            return TokenCache();
        }

        return cache;
    }
}

void print(stream& io, const basil::Token& t) {
    print(io, "[", t.line, ":", t.column, "]\t");
    print(io, "token ", t.type); // token info
    print(io, " (", basil::TOKEN_NAMES[t.type], ")"); // type desc
    if (t.value.size() > 0) {
        print(io, ":\t\"", t.value, "\"");
    }
}

void print(const basil::Token& t) {
    print(_stdout, t);
}

void print(stream& io, const basil::TokenCache& c) {
    println(io, c.size(), " tokens");
    for (const basil::Token& t : c) println(io, t);
    println(io, "----");
}

void print(const basil::TokenCache& c) {
    print(_stdout, c);
}

void read(stream& io, basil::Token& t) {
    string key;
    u32 line, column, type;
    ustring value;
    while (isspace(io.peek())) io.read(); // consume leading spaces

    buffer num;
    io.read();
    while (io.peek() != ':') num.write(io.read());
    io.read(), num.write(' ');
    while (io.peek() != ']') num.write(io.read());
    io.read();

    fread(num, line, column);

    read(io, key, type); // read prefix and info

    while (io.peek() != ')') io.read(); // consume type desc
    io.read();

    if (io.peek() == ':') { // if value present
        while (io.peek() != '"') io.read(); // consume preceding junk
        io.read();

        while (io.peek() != '"') { // read value string
            uchar c;
            read(io, c);
            value += c;
        }
        io.read(); // consume closing quote
    }
    t = basil::Token(value, type, line, column);
}

void read(basil::Token& t) {
    read(_stdin, t);
}

void read(stream& io, basil::TokenCache& c) {
    while (isspace(io.peek())) io.read(); // consume leading spaces

    u32 count;
    string trash;
    read(io, count, trash); // remove header

    for (u32 i = 0; i < count; ++ i) {
        basil::Token t;
        read(io, t);
        c.push(t);
    }
    read(io, trash); // consume "----"
}

void read(basil::TokenCache& c) {
    read(_stdin, c);
}