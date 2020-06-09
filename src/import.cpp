#include "import.h"
#include "lex.h"
#include "term.h"
#include "parse.h"
#include "errors.h"
#include "value.h"

namespace basil {
    map<string, Module*> modules;

    Module::Module(const string& path, Source* src, ProgramTerm* body, Stack* env):
        _path(path), _src(src), _body(body), _env(env) {
        //
    }

    Module::~Module() {
        delete _src;
        delete _body;
        delete _env;
    }

    void Module::useIn(Stack& ctx, u32 line, u32 column) {
        for (auto& p : _body->scope().scope()) {
            auto it = ctx.nearestScope().find(p.first);
            if (it != ctx.nearestScope().end()) {
                err(PHASE_TYPE, line, column,
                    "Module '", _path, "' redefines variable '",
                    p.first, "' from the local environment.");
                return;
            }
            ctx.nearestScope().put(p.first, p.second);
        }
    }

    void freeModules() {
        for (auto& p : modules) delete p.second;
    }

    Module* loadModule(const char* path, u32 line, u32 column) {
        auto it = modules.find(path);
        if (it != modules.end()) {
            return it->second;
        }

        if (!exists(path)) {
            err(PHASE_TYPE, line, column,
                "Could not find module at relative path '",
                path, "'.");
            return nullptr;
        }
        Source* src = new Source(path);
        Source* prev = currentSource();
        useSource(src);
        TokenCache tok = lex(*src);
        if (countErrors()) {
            printErrors(_stdout);
            delete src;
            return nullptr;
        }
        TokenCache::View v = tok.view();
        ProgramTerm* module = parseFull(v);
        if (countErrors()) {
            printErrors(_stdout);
            delete src;
            delete module;
            return nullptr;
        }
        Stack* env = new Stack(nullptr);
        module->eval(*env);
        if (countErrors()) {
            printErrors(_stdout);
            delete src;
            delete env;
            delete module;
            return nullptr;
        }
        useSource(prev);
        return modules[path] = new Module(path, src, module, env);
    }
}