#ifndef BASIL_ERROR_H
#define BASIL_ERROR_H

#include "defs.h"
#include "io.h"
#include "vec.h"

namespace basil {
    struct Error {
        const Source* src;
        u32 line, column;
        buffer message;

        Error();
        void format(stream& io) const;
    };

    enum Phase {
        PHASE_LEX, PHASE_PARSE, PHASE_TYPE
    };

    void prefixPhase(buffer& b, Phase phase);
    void reportError(const Error& error);
    void useSource(Source* src);
    u32 countErrors();
    void printErrors(stream& io);

    void catchErrors();
    void releaseErrors();
    void discardErrors();
    Error& lastError();

    template<typename... Args>
    void err(Phase phase, u32 line, u32 column, const Args&... args) {
        buffer b;
        fprint(b, "(", line, ":", column, ") ", args...);
        Error e;
        e.line = line, e.column = column;
        e.message = b;
        e.src = nullptr;
        reportError(e);
    }

    template<typename... Args>
    void err(Phase phase, const Source* src, u32 line, u32 column, const Args&... args) {
        buffer b;
        fprint(b, "(", line, ":", column, ") ", args...);
        Error e;
        e.line = line, e.column = column;
        e.message = b;
        e.src = src;
        reportError(e);
    }

    template<typename... Args>
    void note(Phase phase, u32 line, u32 column, const Args&... args) {
        buffer b;
        fprint(b, "(", line, ":", column, ") - ", args...);
        fprint(lastError().message, "\n", b);
    }
}

void print(stream& io, const basil::Error& e);
void print(const basil::Error& e);

#endif