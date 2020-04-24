#include "errors.h"
#include "source.h"

namespace basil {
    Error::Error(): src(nullptr), line(0), column(0) {
        //
    }

    void Error::format(stream& io) const {
        println(io, message);
        if (src) {
            print(io, "    ", src->line(line - 1));
            print(io, "    ");
            for (u32 i = 0; i < column - 1; i ++) print(io, " ");
            println(io, "^");
        }
    }   

    static vector<Error> errors;
    static set<ustring> messages;

    static vector<vector<Error>> errorFrames;
    static vector<set<ustring>> frameMessages;

    void catchErrors() {
        errorFrames.push({});
        frameMessages.push({});
    }

    void releaseErrors() {
        vector<Error> removed = errorFrames.back();
        errorFrames.pop();
        frameMessages.pop();
        for (const Error& e : removed) reportError(e);
    }

    void discardErrors() {
        // if (countErrors()) printErrors(_stdout);
        errorFrames.pop();
        frameMessages.pop();
    }

    void prefixPhase(buffer& b, Phase phase) {
        switch (phase) {
            case PHASE_LEX:
                fprint(b, "[TOKEN ERROR]");
                break;
            case PHASE_PARSE:
                fprint(b, "[PARSE ERROR]");
                break;
            case PHASE_TYPE:
                fprint(b, "[TYPE ERROR]");
                break;
            default:
                break;
        }
    }

    static Source* _src;

    void useSource(Source* src) {
        _src = src;
    }

    void reportError(const Error& error) {
        ustring s;
        buffer b = error.message;
        fread(b, s);
        vector<Error>& es = errorFrames.size() 
            ? errorFrames.back() : errors;
        set<ustring>& ms = frameMessages.size() 
            ? frameMessages.back() : messages;
        if (ms.find(s) == ms.end()) {
            ms.insert(s);
            es.push(error);
            if (!es.back().src && _src) es.back().src = _src;
        }
    }

    u32 countErrors() {
        vector<Error>& es = errorFrames.size() 
            ? errorFrames.back() : errors;
        return es.size();
    }

    void printErrors(stream& io) {
        println(io, countErrors(), " error", countErrors() != 1 ? "s" : "");
        for (const Error& e : (errorFrames.size() 
            ? errorFrames.back() : errors)) print(io, e);
    }
}

void print(stream& io, const basil::Error& e) {
    e.format(io);
}

void print(const basil::Error& e) {
    e.format(_stdout);
}