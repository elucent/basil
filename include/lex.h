#ifndef BASIL_LEX_H
#define BASIL_LEX_H

#include "defs.h"
#include "str.h"
#include "utf8.h"
#include "vec.h"
#include "io.h"
#include "source.h"
#include "errors.h"

namespace basil {
    extern const u32 
        TOKEN_NONE, 
        TOKEN_IDENT, 
        TOKEN_STRING, 
        TOKEN_CHAR, 
        TOKEN_NUMBER, 
        TOKEN_SYMBOL,
        TOKEN_LPAREN, 
        TOKEN_RPAREN, 
        TOKEN_LBRACE, 
        TOKEN_RBRACE,
        TOKEN_LBRACK, 
        TOKEN_RBRACK,
        TOKEN_COLON, 
        TOKEN_SEMI, 
        TOKEN_NEWLINE, 
        TOKEN_ASSIGN,
        TOKEN_LAMBDA,
        TOKEN_DOT,
        TOKEN_PLUS,
        TOKEN_MINUS,
        TOKEN_EVAL,
        TOKEN_REF, 
        TOKEN_BOOL,
        TOKEN_QUOTE;
    
    extern const char* TOKEN_NAMES[24];

    struct Token {
        ustring value;
        u32 type;
        u32 line, column;

        Token();

        Token(const ustring& value_in, u32 type_in, 
              u32 line_in, u32 column_in);

        operator bool() const;
    };

    class TokenCache {
        const static Token NONE;
        vector<Token> tokens;
        Source* _src;
    public:
        TokenCache(Source* src = nullptr);
        void push(const Token& t);

        class View {
            TokenCache* _cache;
            u32 i;
        public:
            View(TokenCache* cache);
            View(TokenCache* cache, u32 index);

            const Token& read();
            const Token& peek() const;
            TokenCache& cache();
            operator bool() const;
        };

        Source* source();
        View view();
        View expand(stream& io);
        u32 size() const;
        const Token* begin() const;
        const Token* end() const;
    };

    Token scan(Source::View& view);
    TokenCache lex(Source& src);
}

void print(stream& io, const basil::Token& t);
void print(const basil::Token& t);
void print(stream& io, const basil::TokenCache& c);
void print(const basil::TokenCache& c);
void read(stream& io, basil::Token& t);
void read(basil::Token& t);
void read(stream& io, basil::TokenCache& c);
void read(basil::TokenCache& c);

#endif