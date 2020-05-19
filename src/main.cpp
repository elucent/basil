#include "vec.h"
#include "utf8.h"
#include "hash.h"
#include "stdio.h"
#include "stdlib.h"
#include "source.h"
#include "time.h"
#include "lex.h"
#include "parse.h"
#include "type.h"
#include "term.h"
#include "value.h"
#include "ir.h"

using namespace basil;

u32 LEX = 1,
    PARSE = 2,
    AST = 3,
    IR = 4,
    ASM = 5;

bool interactive = true;
bool silent = false;
u32 level = ASM;
Source* src = nullptr;
ustring outfile;
ustring infile;

ustring stripEnding(const ustring& path) {
    ustring n;
    i64 lastDot = -1;
    for (u32 i = 0; i < path.size(); i ++) {
        if (path[i] == '.') lastDot = i;
    }
    for (u32 i = 0; i < path.size(); i ++) {
        if (i < lastDot || lastDot <= 0) n += path[i];
    }
    return n;
}

void repl() {
    CodeGenerator gen;
    useSource(src);
    TokenCache cache(src);
    ProgramTerm* program = new ProgramTerm({}, 1, 1);
    while (!countErrors()) {
        if (level < LEX) return;
        print("? ");
        TokenCache::View view = cache.expand(_stdin);
        if (countErrors()) {
            printErrors(_stdout);
            continue;
        }
        else if (level == LEX && !silent) {
            println("");
            println(_stdout, cache);
            println("");
        }
        
        if (view.peek().type == TOKEN_IDENT && view.peek().value == "quit") {
            println("Leaving REPL...");
            break;
        }

        if (level < PARSE) continue;

        Term* t = parse(view, true);
        if (countErrors()) {
            printErrors(_stdout);
            continue;
        }
        else if (level == PARSE && !silent) {
            println("");
            t->format(_stdout);
            println("");
        }

        if (level < AST) continue;

        Stack* s = new Stack();
        program->add(t);
        program->evalChild(*s, t);
        
        if (countErrors()) {
            printErrors(_stdout);
            continue;
        }
        else if (level == AST && !silent) {
            println("");
            for (Value* v : *s) v->format(_stdout);
            println("");
        }

        if (level < IR) continue;

        for (Value* v : *s) v->gen(program->scope(), gen, gen);
        gen.finalize(gen);
        
        if (level < ASM) {
            if (!silent) gen.format(_stdout);
            continue;
        }

        gen.allocate();
        buffer text, data;
        gen.emitX86(text, data);
        if (!silent) fprint(_stdout, data, text);

        delete s;
    }

    delete program;
    delete src;
}

int parseArgs(int argc, char** argv) {
    while (argc) {
        if (string(*argv) == "-o") {
            if (argc == 1) {
                println("Error: '-o' was provided without an argument.");
                return 1;
            }
            outfile = ustring(*(--argc, ++argv));
        }
        else if (string(*argv) == "-silent") {
            silent = true;
        }
        else if (string(*argv) == "-ir") {
            level = IR;
        }
        else if (string(*argv) == "-ast") {
            level = AST;
        }
        else if (string(*argv) == "-lex") {
            level = LEX;
        }
        else if (string(*argv) == "-parse") {
            level = PARSE;
        }
        else {
            infile = ustring(*argv);
            interactive = false;
            src = new Source(*argv);
        }
        --argc, ++ argv;
    }
    if (!src) {
        if (interactive) src = new Source();
    }
    return 0;
}

int main(int argc, char** argv) {
    if (auto code = parseArgs(argc - 1, argv + 1)) return code;
    if (outfile.size() == 0) {
        if (infile.size() > 0) {
            outfile = stripEnding(infile) + ".s";
        }
        else outfile = "output.s";
    }

    if (interactive) {
        repl();
        return countErrors();
    }

    useSource(src);
    TokenCache cache(src);
    ProgramTerm* program = new ProgramTerm({}, 1, 1);
    Stack s;    
    CodeGenerator gen;
    
    cache = lex(*src);
    if (countErrors()) { 
        printErrors(_stdout);
        return 1;
    }
    else if (level == LEX && !silent) println(""), println(_stdout, cache), println("");

    TokenCache::View v = cache.view();

    if (level >= PARSE) {
        program = parseFull(v);
        if (countErrors()) {
            printErrors(_stdout);
            return 1;
        }
        else if (level == PARSE && !silent) program->format(_stdout);
    }
    
    if (level >= AST) {
        program->eval(s);
        if (countErrors()) {
            printErrors(_stdout);
            return 1;
        }
        else if (level == AST && !silent) for (Value* v : s) v->format(_stdout);
    }
    
    if (level >= IR) {
        for (Value* v : s) v->gen(program->scope(), gen, gen);
        gen.finalize(gen);
        
        if (level < ASM) {
            if (!silent) gen.format(_stdout);
        }
        else {
            gen.allocate();
            buffer text, data;
            gen.emitX86(text, data);
            if (!silent) fprint(_stdout, data, text);
        }
    }
    
    delete src;
    return countErrors();
}
