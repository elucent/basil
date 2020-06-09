#include "parse.h"
#include "vec.h"
#include "term.h"
#include "io.h"

namespace basil {
    static bool repl_mode;

    template<typename... Args>
    static void err(TokenCache::View& t, Args... message) {
        if (t.cache().source()) {
            err(PHASE_PARSE, t.cache().source(), t.peek().line, 
                t.peek().column, message...);
        }
        else {
            err(PHASE_PARSE, t.peek().line, t.peek().column, message...);
        }
    }

    void parsePrimary(vector<Term*>& terms, TokenCache::View& view, u32 indent);

    u32 parseChunk(vector<Term*>& terms, TokenCache::View& view, 
                   u32 indent, bool consume = true) {
        const Token first = view.peek();
        while (true) {
            if (view.peek().type == TOKEN_NEWLINE
                || view.peek().type == TOKEN_SEMI
                || view.peek().type == TOKEN_RPAREN
                || view.peek().type == TOKEN_RBRACK
                || view.peek().type == TOKEN_RBRACE
                || !view.peek()) {
                if (view.peek().type == TOKEN_SEMI && consume) 
                    return view.read().type;
                return view.peek().type;
            }
            parsePrimary(terms, view, indent);
        }
        return TOKEN_NONE;
    }

    u32 parseLine(vector<Term*>& terms, TokenCache::View& view, 
                  u32 indent, bool consume = true) {
        vector<Term*> contents;
        u32 l = view.peek().line, c = view.peek().column;
        auto terminator = parseChunk(contents, view, indent);
        while (terminator == TOKEN_SEMI) {
            if (contents.size()) terms.push(new BlockTerm(contents, l, c));
            l = view.peek().line, c = view.peek().column;
            contents.clear();
            terminator = parseChunk(contents, view, indent);
        }
        if (contents.size()) {
            if (terms.size()) terms.push(new BlockTerm(contents, l, c));
            else for (Term* t : contents) terms.push(t);
        }

        // consume newline
        if (consume && terminator == TOKEN_NEWLINE) view.read();
        return terminator;
    }

    u32 parseEnclosed(vector<Term*>& terms, TokenCache::View& view, 
                      u32 closer, u32 indent) {
        vector<Term*> contents;
        u32 l = view.peek().line, c = view.peek().column;
        auto terminator = parseLine(contents, view, indent);
        while (terminator == TOKEN_NEWLINE) {
            if (contents.size()) terms.push(new BlockTerm(contents, l, c));
            l = view.peek().line, c = view.peek().column;
            contents.clear();
            terminator = parseLine(contents, view, indent);
            if (terminator == TOKEN_NONE && repl_mode) {
                print(". ");
                view.cache().expand(_stdin);
                terminator = TOKEN_NEWLINE;
            }
        }
        if (contents.size()) {
            if (terms.size() > 0) terms.push(new BlockTerm(contents, l, c));
            else for (Term* t : contents) terms.push(t);
        }

        // consume closer
        if (terminator == TOKEN_NONE) {
            err(view, "Unexpected end of input.");
        }
        else if (terminator != closer) {
            err(view, "Expected '", TOKEN_NAMES[closer], "', found '",
                TOKEN_NAMES[terminator], "' at end of enclosed block.");
        }
        view.read();
        return terminator;
    }

    void parseIndented(vector<Term*>& terms, TokenCache::View& view, 
                       u32 indent, u32 prev) {
        const Token first = view.peek();

        vector<Term*> contents;
        u32 l = view.peek().line, c = view.peek().column;
        auto terminator = parseLine(contents, view, c);
        
        if ((!view.peek() || 
             (terminator == TOKEN_NONE && view.peek().column > prev))
             && repl_mode) {
            print(". ");
            view.cache().expand(_stdin);
        }

        while (view.peek() && view.peek().column > prev) {
            if (contents.size()) terms.push(new BlockTerm(contents, l, c));
            l = view.peek().line, c = view.peek().column;
            contents.clear();
            terminator = parseLine(contents, view, c, false);
            if (view.peek().type == TOKEN_NEWLINE && view.peek().column > prev)
                view.read();
            if ((!view.peek() || 
                 (terminator == TOKEN_NONE && view.peek().column > prev))
                 && repl_mode) {
                print(". ");
                view.cache().expand(_stdin);
            }
        }
        if (contents.size()) {
            if (terms.size() > 0) terms.push(new BlockTerm(contents, l, c));
            else for (Term* t : contents) terms.push(t);
        }
    }

    void parsePrimary(vector<Term*>& terms, TokenCache::View& view, 
                      u32 indent) {
        buffer b;
        const Token t = view.peek();
        if (t.type == TOKEN_NUMBER) {
            view.read();
            fprint(b, t.value);
            for (u32 i = 0; i < t.value.size(); ++ i) {
                if (t.value[i] == '.') {
                    double v;
                    fread(b, v);
                    terms.push(new RationalTerm(v, t.line, t.column));
                    return;    
                }
            }
            i64 v;
            fread(b, v);
            terms.push(new IntegerTerm(v, t.line, t.column));
        }
        else if (t.type == TOKEN_STRING) {
            view.read();
            terms.push(new StringTerm(t.value, t.line, t.column));
        }
        else if (t.type == TOKEN_CHAR) {
            view.read();
            terms.push(new CharTerm(t.value[0], t.line, t.column));
        }
        else if (t.type == TOKEN_BOOL) {
            view.read();
            terms.push(new BoolTerm(t.value == "true", t.line, t.column));
        }
        else if (t.type == TOKEN_IDENT) {
            view.read();
            terms.push(new VariableTerm(t.value, t.line, t.column));
        }
        else if (t.type == TOKEN_LPAREN) {
            view.read();
            vector<Term*> contents;
            parseEnclosed(contents, view, TOKEN_RPAREN, indent);
            if (contents.size() == 0) 
                terms.push(new VoidTerm(t.line, t.column));
            else
                terms.push(new BlockTerm(contents, t.line, t.column));
        }
        else if (t.type == TOKEN_LBRACE) {
            view.read();
            vector<Term*> contents;
            parseEnclosed(contents, view, TOKEN_RBRACE, indent);
            Term* vals = new BlockTerm({
                new VariableTerm("record", t.line, t.column),
                new BlockTerm(contents, t.line, t.column)
            }, t.line, t.column);
            terms.push(vals);
        }
        else if (t.type == TOKEN_LBRACK) {
            view.read();
            vector<Term*> contents;
            parseEnclosed(contents, view, TOKEN_RBRACK, indent);
            Term* vals = new BlockTerm({
                new VariableTerm("array", t.line, t.column),
                new BlockTerm(contents, t.line, t.column)
            }, t.line, t.column);
            terms.push(vals);
        }
        else if (t.type == TOKEN_QUOTE) {
            view.read();
            if (view.peek().type == TOKEN_LAMBDA 
                || view.peek().type == TOKEN_ASSIGN) {
                err(view, "Cannot quote operator '", view.peek().value, "'.");
                return;
            }
            vector<Term*> temp;
            parsePrimary(temp, view, indent);
            if (temp.size() == 0) {
                err(view, "Quote prefix ':' requires operand, none provided.");
                return;
            }
            terms.push(new BlockTerm({
                new VariableTerm("quote", t.line, t.column),
                temp[0]
            }, t.line, t.column));
        }
        else if (t.type == TOKEN_MINUS) {
            view.read();
            vector<Term*> temp;
            parsePrimary(temp, view, indent);
            terms.push(new BlockTerm({
                new IntegerTerm(0, t.line, t.column),
                new VariableTerm("-", t.line, t.column),
                temp[0]
            }, t.line, t.column));
        }
        else if (t.type == TOKEN_PLUS) {
            view.read();
            vector<Term*> temp;
            parsePrimary(temp, view, indent);
            terms.push(new BlockTerm({
                new IntegerTerm(0, t.line, t.column),
                new VariableTerm("+", t.line, t.column),
                temp[0]
            }, t.line, t.column));
        }
        else if (t.type == TOKEN_EVAL) {
            view.read();
            vector<Term*> temp;
            parsePrimary(temp, view, indent);
            terms.push(new BlockTerm({
                new VariableTerm("eval", t.line, t.column),
                temp[0]
            }, t.line, t.column));
        }
        else if (t.type == TOKEN_REF) {
            view.read();
            vector<Term*> temp;
            parsePrimary(temp, view, indent);
            terms.push(new BlockTerm({
                new VariableTerm("~", t.line, t.column),
                temp[0]
            }, t.line, t.column));
        }
        else if (t.type == TOKEN_DOT) {
            view.read();
            if (!terms.size()) {
                err(view, "Expected term to the left of dot.");
                return;
            }
            Term* left = terms.back();
            terms.pop();
            vector<Term*> temp;
            parsePrimary(temp, view, indent);
            if (!temp.size()) {
                err(view, "Expected term to the right of dot.");
                return;
            }
            terms.push(new BlockTerm({
                left,
                temp.size() == 1 ? temp[0]
                    : new BlockTerm(temp, temp[0]->line(), temp[0]->column())
            }, t.line, t.column));
        }
        else if (t.type == TOKEN_LAMBDA) {
            if (terms.size() == 0) {
                err(view, "No argument provided in function definition.");
                return;
            }
            view.read();
            Term* arg = new BlockTerm(terms, terms[0]->line(), terms[0]->column());
            terms.clear();
            vector<Term*> temp;
            if (view.peek().type == TOKEN_NEWLINE 
                || view.peek().type == TOKEN_NONE) {
                view.read();
                u32 c = view.peek().column;
                parseIndented(temp, view, c, indent);
            }   
            else parseLine(temp, view, indent, false);
            terms.push(new BlockTerm({
                new VariableTerm(
                    "lambda", 
                    t.line, t.column
                ),
                arg,
                temp.size() == 1 ? temp[0]
                    : new BlockTerm(temp, temp[0]->line(), temp[0]->column())
            }, t.line, t.column));
        }
        else if (t.type == TOKEN_ASSIGN) {
            view.read();
            if (terms.size() == 0) {
                err(view, "No left term provided to assignment operator.");
                return;
            }
            Term* dst = terms.size() == 1 ? terms[0]
                : new BlockTerm(terms, terms[0]->line(), terms[0]->column());
            terms.clear();
            vector<Term*> temp;
            if (view.peek().type == TOKEN_NEWLINE 
                || view.peek().type == TOKEN_NONE) {
                view.read();
                u32 c = view.peek().column;
                parseIndented(temp, view, c, indent);
            }   
            else parseChunk(temp, view, indent, false);
            terms.push(new BlockTerm({
                new VariableTerm("assign", t.line, t.column),
                dst,
                temp.size() == 1 ? temp[0]
                    : new BlockTerm(temp, temp[0]->line(), temp[0]->column())
            }, t.line, t.column));
        }
        else if (t.type == TOKEN_COLON) {
            view.read();
            vector<Term*> temp;
            if (view.peek().type == TOKEN_NEWLINE 
                || view.peek().type == TOKEN_NONE) {
                view.read();
                u32 c = view.peek().column;
                parseIndented(temp, view, c, indent);
            }   
            else parseChunk(temp, view, indent, false);
            terms.push(new BlockTerm(temp, t.line, t.column));
        }
        else if (t.type == TOKEN_NEWLINE) {
            view.read();
            if (repl_mode) {
                print(". ");
                view.cache().expand(_stdin);
            }
        }
        else {
            view.read();
            err(view, "Unexpected token '", t.value, "'.");
        }
    }

    Term* parse(TokenCache::View& view, bool repl) {
        repl_mode = repl;
        vector<Term*> terms;
        parseLine(terms, view, 1);
        
        if (countErrors() > 0) return nullptr;
        return terms.size() == 1 ? terms[0] : 
            new BlockTerm(terms, terms[0]->line(), terms[0]->column());
    }

    ProgramTerm* parseFull(TokenCache::View& view, bool repl) {
        repl_mode = repl;
        ProgramTerm* p = new ProgramTerm({}, view.peek().line, 
                                         view.peek().column);
        while (view.peek()) {
            vector<Term*> terms;
            parseLine(terms, view, 1);
            if (terms.size()) {
                p->add(terms.size() == 1 ? terms[0] : 
                    new BlockTerm(terms, terms[0]->line(), terms[0]->column()));
            }
        }

        if (countErrors() > 0) return nullptr;

        return p;
    }
}