#ifndef BASIL_PARSE_H
#define BASIL_PARSE_H

#include "defs.h"
#include "lex.h"

namespace basil {
    Term* parse(TokenCache::View& view, bool repl = false);
    ProgramTerm* parseFull(TokenCache::View& view, bool repl = false);
}

#endif