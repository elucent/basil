#include "defs.h"
#include "hash.h"
#include "str.h"

namespace basil {
    extern map<string, Module*> modules;

    class Module {
        string _path;
        Source* _src;
        ProgramTerm* _body;
        Stack* _env;
    public:
        Module(const string& path, Source* src, ProgramTerm* body, Stack* env);
        ~Module();
        void useIn(Stack& stack, u32 line, u32 column);
    };

    void freeModules();
    Module* loadModule(const char* path, u32 line, u32 column);
}