#include "x64.h"
#include "hash.h"

namespace basil {
    namespace x64 {
        const char* CONDITION_NAMES[8] = {
            "e",
            "ne",
            "l",
            "le",
            "g",
            "ge",
            "z",
            "nz"
        };

        Size typeSize(const Type* t) {
            switch (t->size()) {
                case 0:
                    return VOID;
                case 1:
                    return BYTE;
                case 2:
                    return WORD;
                case 4:
                    if (t->is<NumericType>() && 
                        t->as<NumericType>()->floating())
                        return SINGLE;
                    return DWORD;
                case 8:
                    if (t->is<NumericType>() && 
                        t->as<NumericType>()->floating())
                        return DOUBLE;
                    return QWORD;
                default:
                    return ERROR;
            }
        }
        
        namespace printer {
            void printSized(buffer& b, const char* opcode, Size kind) {
                fprint(b, opcode);
                switch (kind) {
                    case BYTE:
                        return fprint(b, 'b');
                    case WORD:
                        return fprint(b, 'w');
                    case DWORD:
                        return fprint(b, 'l');
                    case QWORD:
                        return fprint(b, 'q');
                    case SINGLE:
                        return fprint(b, "ss");
                    case DOUBLE:
                        return fprint(b, "sd");
                    default:
                        return;
                }
            }

            void printUnsized(buffer& b, const char* opcode) {
                fprint(b, opcode);
            }

            void printSized(buffer& b, Register reg, Size kind) {
                fprint(b, "%");
                switch (reg) {
                    case RAX:
                    case RCX:
                    case RDX:
                    case RBX:
                        if (kind == QWORD) fprint(b, 'r');
                        else if (kind == DWORD) fprint(b, 'e');
                        fprint(b, REGISTER_NAMES[reg][1]);
                        if (kind == BYTE) fprint(b, 'l');
                        else fprint(b, 'x');
                        break;
                    case RBP:
                    case RSP:
                    case RSI:
                    case RDI:
                        if (kind == QWORD) fprint(b, 'r');
                        else if (kind == DWORD) fprint(b, 'e');
                        fprint(b, REGISTER_NAMES[reg] + 1);
                        if (kind == BYTE) fprint(b, 'l');
                        break;
                    case R8:
                    case R9:
                    case R10:
                    case R11:
                    case R12:
                    case R13:
                    case R14:
                    case R15:
                        fprint(b, REGISTER_NAMES[reg]);
                        if (kind == DWORD) fprint(b, 'd');
                        else if (kind == WORD) fprint(b, 'w');
                        else if (kind == BYTE) fprint(b, 'b');
                        break;
                    case XMM0:
                    case XMM1:
                    case XMM2:
                    case XMM3:
                    case XMM4:
                    case XMM5:
                    case XMM6:
                    case XMM7:
                        fprint(b, "xmm", (u8)reg - 32);
                        break;
                    default:
                        break;
                }
            }

            void printArg(buffer& b, Location* loc) {
                if (loc->imm)
                    loc->imm->emitX86Arg(b);
                else if (loc->segm == IMMEDIATE)
                    fprint(b, "$", loc->off);
                else if (loc->segm == STACK)
                    fprint(b, loc->off, "(%rbp)"); 
                else if (loc->segm == REGISTER)
                    printSized(b, loc->reg, typeSize(loc->type));
                else if (loc->segm == REGISTER_RELATIVE) {
                    fprint(b, loc->off, "(");
                    printSized(b, loc->reg, QWORD);
                    fprint(b, ")");
                }
                else if (loc->segm == RELATIVE) {
                    if (loc->src->segm == STACK) {
                        Location newloc(STACK, loc->src->off + loc->off, loc->type);
                        printArg(b, &newloc);
                    }
                    else if (loc->src->segm == REGISTER_RELATIVE) {
                        Location newloc(loc->src->reg, loc->src->off + loc->off, loc->type);
                        printArg(b, &newloc);
                    }
                }
            }

            void indent(buffer& b) {
                fprint(b, "    ");
            }
            
            void intconst(buffer& text, buffer& data, i64 value) {
                indent(data);
                fprintln(data, ".quad ", value);
            }
            
            void fconst(buffer& text, buffer& data, double value) {
                indent(data);
                fprintln(data, ".double ", value);
            }

            void strconst(buffer& text, buffer& data, const ustring& value) {
                indent(data);
                buffer b;
                fprint(b, escape(value));
                for (int i = value.size(); i % 8 != 0; i ++) b.write('\\'), b.write('0');
                fprintln(data, ".ascii \"", b, "\"");
            }

            void text(buffer& text, buffer& data) {
                fprintln(text, ".text");
            }

            void data(buffer& text, buffer& data) {
                fprintln(data, ".data");
            }

            void label(buffer& text, buffer& data, Section section, const ustring& name, bool global) {
                if (global) fprintln((section == TEXT ? text : data), ".global ", name);
                fprintln((section == TEXT ? text : data), name, ":"); 
            }

            void binary(buffer& text, buffer& data, 
                Location* src, Location* dst, const char* opcode,
                void (*self)(buffer&, buffer&, Location*, Location*), bool sized = true) {
                if (dst->imm) { // don't target immediates
                    Location* tmp = dst;
                    dst = src;
                    src = tmp;
                }
                if (src->segm == REGISTER || dst->segm == REGISTER) {
                    indent(text);
                    if (sized) printSized(text, opcode, typeSize(src->type));
                    else printUnsized(text, opcode);
                    fprint(text, " ");
                    printArg(text, src);
                    fprint(text, ", ");
                    printArg(text, dst);
                    fprintln(text, "");
                }
                else {
                    Location rax(RAX, src->type);
                    mov(text, data, src, &rax);
                    self(text, data, &rax, dst);
                }
            }

            void mov(buffer& text, buffer& data, Location* src, Location* dst) {
                if (*src == *dst) return;
                binary(text, data, src, dst, "mov", mov);
            }

            void add(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "add", add);
            }

            void sub(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "sub", sub);
            }

            void imul(buffer& text, buffer& data, Location* src, Location* dst) {
                Location* target = dst;
                if (dst->segm != REGISTER) {
                    if (src->segm == REGISTER) {
                        Location* tmp = dst;
                        dst = src;
                        src = tmp;                    
                    }
                    else {
                        Location rax(RAX, dst->type);
                        target = &rax;
                    }
                }
                binary(text, data, src, target, "imul", imul);
                if (target != dst) mov(text, data, target, dst);
            }

            void mul(buffer& text, buffer& data, Location* src, Location* dst) {
                Location* target = dst;
                if (dst->segm != REGISTER) {
                    if (src->segm == REGISTER) {
                        Location* tmp = dst;
                        dst = src;
                        src = tmp;                    
                    }
                    else {
                        Location rax(RAX, dst->type);
                        target = &rax;
                    }
                }
                binary(text, data, src, target, "mul", mul);
                if (target != dst) mov(text, data, target, dst);
            }

            void idiv(buffer& text, buffer& data, Location* src) {
                indent(text);
                printSized(text, "idiv", typeSize(src->type));
                fprint(text, " ");
                printArg(text, src);
                fprintln(text, "");
            }

            void div(buffer& text, buffer& data, Location* src) {
                indent(text);
                printSized(text, "div", typeSize(src->type));
                fprint(text, " ");
                printArg(text, src);
                fprintln(text, "");
            }

            void fdiv(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "div", fdiv);
            }
            
            void cmp(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "cmp", cmp);
            }

            void and_(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "and", and_);
            }

            void or_(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "or", or_);
            }

            void xor_(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "xor", xor_);
            }

            void not_(buffer& text, buffer& data, Location* operand) {
                indent(text);
                printSized(text, "not", typeSize(operand->type));
                fprint(text, " ");
                printArg(text, operand);
                fprintln(text, "");
            }

            void movsx(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "movsx", movsx);
            }

            void movzx(buffer& text, buffer& data, Location* src, Location* dst) {
                binary(text, data, src, dst, "movzx", movzx);
            }

            void cvt(buffer& text, buffer& data, Location* src, Location* dst,
                const char* opcode, void (*self)(buffer&, buffer&, Location*, Location*),
                bool sized = true) {
                Location* target = dst;
                if (dst->segm != REGISTER) {
                    Location rax(RAX, dst->type);
                    Location xmm7(XMM7, dst->type);
                    if (dst->type->is<NumericType>() &&
                        dst->type->as<NumericType>()->floating())
                        target = &xmm7;
                    else target = &rax;
                }
                binary(text, data, src, target, opcode, self, sized);
                if (target != dst) mov(text, data, target, dst);
            }

            void cvttsd2si(buffer& text, buffer& data, Location* src, Location* dst) {
                cvt(text, data, src, dst, "cvttsd2si", cvttsd2si);
            }

            void cvttss2si(buffer& text, buffer& data, Location* src, Location* dst) {
                cvt(text, data, src, dst, "cvttss2si", cvttss2si);
            }

            void cvtsd2ss(buffer& text, buffer& data, Location* src, Location* dst) {
                cvt(text, data, src, dst, "cvtsd2ss", cvtsd2ss, false);
            }

            void cvtss2sd(buffer& text, buffer& data, Location* src, Location* dst) {
                cvt(text, data, src, dst, "cvtss2sd", cvtss2sd, false);
            }

            void cvtsi2sd(buffer& text, buffer& data, Location* src, Location* dst) {
                cvt(text, data, src, dst, "cvtsi2sd", cvtsi2sd);
            }

            void cvtsi2ss(buffer& text, buffer& data, Location* src, Location* dst) {
                cvt(text, data, src, dst, "cvtsi2ss", cvtsi2ss);
            }

            void lea(buffer& text, buffer& data, const ustring& label, Location* dst) {
                Location* target = dst;
                if (dst->segm != REGISTER) {
                    Location rax(RAX, dst->type);
                    target = &rax;
                }
                indent(text);
                printSized(text, "lea", typeSize(dst->type));
                fprint(text, " ", label, "(%rip), ");
                printArg(text, target);
                fprintln(text, "");
                if (target != dst) mov(text, data, target, dst);
            }

            void lea(buffer& text, buffer& data, Location* addr, Location* dst) {
                Location* target = dst;
                if (dst->segm != REGISTER) {
                    Location rax(RAX, dst->type);
                    target = &rax;
                }
                indent(text);
                printSized(text, "lea", typeSize(dst->type));
                fprint(text, " ");
                printArg(text, addr);
                fprint(text, ", ");
                printArg(text, target);
                fprintln(text, "");
                if (target != dst) mov(text, data, target, dst);
            }

            void jmp(buffer& text, buffer& data, Location* addr) {
                indent(text);
                fprint(text, "jmp *");
                printArg(text, addr);
                fprintln(text, "");
            }

            void jmp(buffer& text, buffer& data, const ustring& label) {
                indent(text);
                fprintln(text, "jmp ", label);
            }

            void jcc(buffer& text, buffer& data, const ustring& label, Condition condition) {
                indent(text);
                fprintln(text, "j", CONDITION_NAMES[condition], " ", label);
            }
            
            void setcc(buffer& text, buffer& data, Location* dst, Condition condition) {
                indent(text);
                fprint(text, "set", CONDITION_NAMES[condition], " ");
                printArg(text, dst);
                fprintln(text, "");
            }

            void syscall(buffer& text, buffer& data) {
                indent(text);
                fprintln(text, "syscall");
            }

            void ret(buffer& text, buffer& data) {
                indent(text);
                fprintln(text, "ret");
            }

            void call(buffer& text, buffer& data, Location* function) {
                indent(text);
                fprint(text, "callq *");
                printArg(text, function);
                fprintln(text, "");
            }

            void call(buffer& text, buffer& data, const ustring& function) {
                indent(text);
                fprintln(text, "callq ", function);
            }

            void push(buffer& text, buffer& data, Location* src) {
                indent(text);
                printSized(text, "push", typeSize(src->type));
                fprint(text, " ");
                printArg(text, src);
                fprintln(text, "");
            }

            void pop(buffer& text, buffer& data, Location* dst) {
                indent(text);
                printSized(text, "pop", typeSize(dst->type));
                fprint(text, " ");
                printArg(text, dst);
                fprintln(text, "");
            }

            void cdq(buffer& text, buffer& data) {
                indent(text);
                fprintln(text, "cdq");
            }
        }

        // namespace assembler {
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
        //     void lea(buffer& text, buffer& data, const ustring& label, Location* dst);
        //     void lea(buffer& text, buffer& data, Location* addr, Location* dst);
        //     void syscall(buffer& text, buffer& data, Location* number);
        //     void syscall(buffer& text, buffer& data, u8 number);
        //     void ret(buffer& text, buffer& data);
        //     void call(buffer& text, buffer& data, Location* function);
        //     void call(buffer& text, buffer& data, const ustring& function);
        //     void push(buffer& text, buffer& data, Location* src);
        //     void pop(buffer& text, buffer& data, Location* dst);
        // }
    }
}