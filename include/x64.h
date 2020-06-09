#ifndef BASIL_X64_H

#include "defs.h"
#include "io.h"
#include "ir.h"
#include "type.h"

namespace basil {
    namespace x64 {
        enum Size {
            VOID,       // 0 bytes
            BYTE,       // 1 byte
            WORD,       // 2 bytes
            DWORD,      // 4 bytes
            QWORD,      // 8 bytes
            SINGLE,     // 4 byte float
            DOUBLE,     // 8 byte float
            ERROR       // unknown type
        };

        enum Condition {
            EQUAL = 0,
            NOT_EQUAL = 1,
            LESS = 2,
            LESS_EQUAL = 3,
            GREATER = 4,
            GREATER_EQUAL = 5,
            ZERO = 6,
            NOT_ZERO = 7
        };

        extern const char* CONDITION_NAMES[8];

        enum Section {
            TEXT,
            DATA
        };
        
        Size typeSize(const Type* t);

        namespace printer {
            void intconst(buffer& text, buffer& data, i64 value);
            void fconst(buffer& text, buffer& data, double value);
            void strconst(buffer& text, buffer& data, const ustring& value);

            void text(buffer& text, buffer& data);
            void data(buffer& text, buffer& data);
            void label(buffer& text, buffer& data, Section section, const ustring& name, bool global);
            void mov(buffer& text, buffer& data, Location* src, Location* dst);
            void add(buffer& text, buffer& data, Location* src, Location* dst);
            void sub(buffer& text, buffer& data, Location* src, Location* dst);
            void imul(buffer& text, buffer& data, Location* src, Location* dst);
            void mul(buffer& text, buffer& data, Location* src, Location* dst);
            void idiv(buffer& text, buffer& data, Location* src);
            void div(buffer& text, buffer& data, Location* src);
            void fdiv(buffer& text, buffer& data, Location* src, Location* dst);
            void cmp(buffer& text, buffer& data, Location* src, Location* dst);
            void and_(buffer& text, buffer& data, Location* src, Location* dst);
            void or_(buffer& text, buffer& data, Location* src, Location* dst);
            void xor_(buffer& text, buffer& data, Location* src, Location* dst);
            void not_(buffer& text, buffer& data, Location* operand);
            void movsx(buffer& text, buffer& data, Location* src, Location* dst);
            void movzx(buffer& text, buffer& data, Location* src, Location* dst);
            void cvttsd2si(buffer& text, buffer& data, Location* src, Location* dst);
            void cvttss2si(buffer& text, buffer& data, Location* src, Location* dst);
            void cvtsd2ss(buffer& text, buffer& data, Location* src, Location* dst);
            void cvtss2sd(buffer& text, buffer& data, Location* src, Location* dst);
            void cvtsi2sd(buffer& text, buffer& data, Location* src, Location* dst);
            void cvtsi2ss(buffer& text, buffer& data, Location* src, Location* dst);
            void lea(buffer& text, buffer& data, const ustring& label, Location* dst);
            void lea(buffer& text, buffer& data, Location* addr, Location* dst);
            void jmp(buffer& text, buffer& data, Location* addr);
            void jmp(buffer& text, buffer& data, const ustring& label);
            void jcc(buffer& text, buffer& data, const ustring& label, Condition condition);
            void setcc(buffer& text, buffer& data, Location* dst, Condition condition);
            void syscall(buffer& text, buffer& data);
            void syscall(buffer& text, buffer& data);
            void ret(buffer& text, buffer& data);
            void call(buffer& text, buffer& data, Location* function);
            void call(buffer& text, buffer& data, const ustring& function);
            void push(buffer& text, buffer& data, Location* src);
            void pop(buffer& text, buffer& data, Location* dst);
            void cdq(buffer& text, buffer& data);
        }

        // namespace assembler {
        //     void intconst(buffer& text, buffer& data, i64 value);
        //     void strconst(buffer& text, buffer& data, const ustring& value);

        //     void text(buffer& text, buffer& data);
        //     void data(buffer& text, buffer& data);
        //     void label(buffer& text, buffer& data, Section section, const ustring& name, bool global);
        //     void mov(buffer& text, buffer& data, Location* src, Location* dst);
        //     void add(buffer& text, buffer& data, Location* src, Location* dst);
        //     void sub(buffer& text, buffer& data, Location* src, Location* dst);
        //     void imul(buffer& text, buffer& data, Location* src, Location* dst);
        //     void mul(buffer& text, buffer& data, Location* src, Location* dst);
        //     void idiv(buffer& text, buffer& data, Location* src);
        //     void div(buffer& text, buffer& data, Location* src);
        //     void cmp(buffer& text, buffer& data, Location* src, Location* dst);
        //     void and_(buffer& text, buffer& data, Location* src, Location* dst);
        //     void or_(buffer& text, buffer& data, Location* src, Location* dst);
        //     void xor_(buffer& text, buffer& data, Location* src, Location* dst);
        //     void cdq(buffer& text, buffer& data);
        //     void lea(buffer& text, buffer& data, const ustring& label, Location* dst);
        //     void lea(buffer& text, buffer& data, Location* addr, Location* dst);
        //     void jmp(buffer& text, buffer& data, const ustring& label);
        //     void jcc(buffer& text, buffer& data, const ustring& label, Condition condition);
        //     void setcc(buffer& text, buffer& data, Location* dst, Condition condition);
        //     void syscall(buffer& text, buffer& data);
        //     void syscall(buffer& text, buffer& data);
        //     void ret(buffer& text, buffer& data);
        //     void call(buffer& text, buffer& data, Location* function);
        //     void call(buffer& text, buffer& data, const ustring& function);
        //     void push(buffer& text, buffer& data, Location* src);
        //     void pop(buffer& text, buffer& data, Location* dst);
        // }
    }
}

#endif