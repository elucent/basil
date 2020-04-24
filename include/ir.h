#ifndef BASIL_IR_H
#define BASIL_IR_H

#include "defs.h"
#include "vec.h"
#include "io.h"
#include "utf8.h"
#include "hash.h"

namespace basil {
    enum Segment {
        INVALID, UNASSIGNED, STACK, DATA, REGISTER, REGISTER_RELATIVE,
        IMMEDIATE, RELATIVE
    };

    enum Register {
        RAX = 0,
        RCX = 1,
        RDX = 2,
        RBX = 3,
        RSP = 4,
        RBP = 5,
        RSI = 6,
        RDI = 7,
        R8 = 8,
        R9 = 9,
        R10 = 10,
        R11 = 11,
        R12 = 12,
        R13 = 13,
        R14 = 14,
        R15 = 15,
        XMM0 = 32,
        XMM1 = 33,
        XMM2 = 34,
        XMM3 = 35,
        XMM4 = 36,
        XMM5 = 37,
        XMM6 = 38,
        XMM7 = 39,
        NONE = 64,
    };

    extern const char* REGISTER_NAMES[65];

    struct Location {
        Segment segm;
        i64 off;
        Register reg;
        const Type* type;
        Data* imm;
        Location* src = nullptr;
        ustring name;

        Location();
        Location(i64 imm);
        Location(Register reg_in, const Type* type_in);
        Location(Register reg_in, i64 offset, const Type* type_in);
        Location(Segment segm_in, i64 offset, const Type* type_in);
        Location(const Type* type_in, const ustring& name_in);
        Location(const Type* type_in, Data* imm, const ustring& name_in);
        Location(const Type* type_in, Location* base, 
                 i64 offset, const ustring& name_in);
        operator bool() const;
        void allocate(Segment segm_in, i64 offset);
        void allocate(Register reg_in);
        void kill();

        bool operator==(const Location& other) const;
    };

    class CodeFrame {
        static Location NONE;
    public:
        virtual Location* stack(const Type* type) = 0;
        virtual Location* stack(const Type* type, const ustring& name) = 0;
        virtual u32 slot(const Type* type) = 0;
        virtual Insn* add(Insn* i) = 0;
        virtual u32 size() const = 0;
        virtual void allocate() = 0;
        virtual Insn* label(const ustring& name) = 0;
        Location* none() const;
    };

    class Function : public CodeFrame{
        u32 _stack;
        u32 _temps;
        vector<Insn*> insns;
        vector<Location> variables;
        map<ustring, Label*> labels;
        ustring _label;
    public:
        Function(const ustring& label);
        Location* stack(const Type* type) override;
        Location* stack(const Type* type, const ustring& name) override;
        u32 slot(const Type* type) override;
        u32 size() const override;
        Insn* add(Insn* i) override;
        Insn* label(const ustring& name) override;
        void allocate() override;
        void format(stream& io);
        const ustring& label() const;

        void emitX86(buffer& text, buffer& data);
    };
    
    class CodeGenerator : public CodeFrame {
        u32 _data;
        u32 _label_ct;
        u32 _stack;
        u32 _temps;
        u32 _datas;
        vector<Data*> datasrcs;
        vector<Insn*> insns;
        vector<Function*> functions;
        vector<Location> variables;
        vector<Location> datavars;
        map<ustring, Label*> labels;
        map<const Type*, Location> arglocs;
        map<const Type*, Location> retlocs;
    public:
        CodeGenerator();
        Location* data(const Type* type, Data* src);
        Function* newFunction();
        ustring newLabel();
        Location* stack(const Type* type) override;
        Location* stack(const Type* type, const ustring& name) override;
        u32 slot(const Type* type) override;
        u32 size() const override;
        Insn* add(Insn* i) override;
        Insn* label(const ustring& name) override;
        void allocate() override;
        Location* locateArg(const Type* type);
        Location* locateRet(const Type* type);
        void serialize();
        void format(stream& io);

        void emitX86(buffer& text, buffer& data);
    };

    class InsnClass {
        const InsnClass* _parent;
    public:
        InsnClass();
        InsnClass(const InsnClass& parent);
        const InsnClass* parent() const;
    };

    class Insn {
        const InsnClass* _insnclass;
    protected:
        Location* _cached;
        virtual Location* lazyValue(CodeGenerator& gen, CodeFrame& frame) = 0;
        set<Location*> _in, _out;
    public:
        static const InsnClass CLASS;
        Insn(const InsnClass* ic = &CLASS);

        Location* value(CodeGenerator& gen);
        Location* value(CodeGenerator& gen, CodeFrame& frame);
        virtual void format(stream& io) = 0;
        const set<Location*>& inset() const;
        const set<Location*>& outset() const;
        virtual bool liveout(CodeFrame& gen, const set<Location*>& out);

        virtual void emitX86(buffer& text, buffer& data);

        template<typename T>
        bool is() const {
            const InsnClass* ic = _insnclass;
            while (ic) {
                if (ic == &T::CLASS) return true;
                ic = ic->parent();
            }
            return false;
        }

        template<typename T>
        T* as() {
            return (T*) this;
        }

        template<typename T>
        const T* as() const {
            return (const T*) this;
        }
    };

    class Data : public Insn {
    protected:
        bool _generated;
    public:
        static const InsnClass CLASS;
        Data(const InsnClass* ic = &CLASS);
        
        virtual void formatConst(stream& io) = 0;
        virtual void emitX86Const(buffer& text, buffer& data) = 0;
        virtual void emitX86Arg(buffer& text) = 0;
        virtual const ustring& label() const = 0;
    };
    
    class IntData : public Data {
        i64 _value;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen, 
                                   CodeFrame& frame) override;
    public:
        static map<i64, Location*> constants;
        static const InsnClass CLASS;
        IntData(i64 value, const InsnClass* ic = &CLASS);

        i64 value() const;
        virtual void format(stream& io) override;
        virtual void formatConst(stream& io) override;
        virtual void emitX86Const(buffer& text, buffer& data) override;
        virtual void emitX86Arg(buffer& text) override;
        virtual const ustring& label() const override;
    };

    class FloatData : public Data {
        double _value;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen, 
            CodeFrame& frame) override;
    public:
        static map<double, Location*> constants;
        static const InsnClass CLASS;
        FloatData(double value, const InsnClass* ic = &CLASS);

        double value() const;
        virtual bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void format(stream& io) override;
        virtual void formatConst(stream& io) override;
        virtual void emitX86Const(buffer& text, buffer& data) override;
        virtual void emitX86Arg(buffer& text) override;
        virtual void emitX86(buffer& text, buffer& data) override;
        virtual const ustring& label() const override;
    };

    // class CharConstInsn;
    
    class StrData : public Data {
        ustring _value;
        ustring _label;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen, 
                                   CodeFrame& frame) override;
    public:
        static map<ustring, Location*> constants;
        static const InsnClass CLASS;
        StrData(const ustring& value, const InsnClass* ic = &CLASS);

        const ustring& value() const;
        virtual void format(stream& io) override;
        virtual void formatConst(stream& io) override;
        virtual void emitX86Const(buffer& text, buffer& data) override;
        virtual void emitX86Arg(buffer& text) override;
        virtual const ustring& label() const override;
    };
    
    class BoolData : public Data {
        bool _value;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen, 
                                   CodeFrame& frame) override;
    public:
        static Location *TRUE, *FALSE;
        static const InsnClass CLASS;
        BoolData(bool value, const InsnClass* ic = &CLASS);

        bool value() const;
        virtual void format(stream& io) override;
        virtual void formatConst(stream& io) override;
        virtual void emitX86Const(buffer& text, buffer& data) override;
        virtual void emitX86Arg(buffer& text) override;
        virtual const ustring& label() const override;
    };

    class BinaryInsn : public Insn {
    protected:
        Location *_lhs, *_rhs;
        virtual Location* lazyValue(CodeGenerator& gen, 
                                   CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        BinaryInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);        
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
    };

    class PrimaryInsn : public Insn {
    protected:
        Location *_operand;
        virtual Location* lazyValue(CodeGenerator& gen, 
                                   CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        PrimaryInsn(Location* operand, const InsnClass* ic = &CLASS);        
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
    };

    class AddInsn : public BinaryInsn {
    public:
        static const InsnClass CLASS;
        AddInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class SubInsn : public BinaryInsn {
    public:
        static const InsnClass CLASS;
        SubInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class MulInsn : public BinaryInsn {
    public:
        static const InsnClass CLASS;
        MulInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class DivInsn : public BinaryInsn {
    public:
        static const InsnClass CLASS;
        DivInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class ModInsn : public BinaryInsn {
    public:
        static const InsnClass CLASS;
        ModInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };
    
    class AndInsn : public BinaryInsn {
    public:
        static const InsnClass CLASS;
        AndInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };
    
    class OrInsn : public BinaryInsn {
    public:
        static const InsnClass CLASS;
        OrInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };
    
    class NotInsn : public PrimaryInsn {
    public:
        static const InsnClass CLASS;
        NotInsn(Location* operand, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class XorInsn : public BinaryInsn {
    public:
        static const InsnClass CLASS;
        XorInsn(Location* lhs, Location* rhs, const InsnClass* ic = &CLASS);

        virtual void format(stream& io) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    // class EqInsn;
    // class NotEqInsn;
    // class LessInsn;
    // class GreaterInsn;
    // class LessEqInsn;
    // class GreaterEqInsn;

    class JoinInsn : public Insn {
        vector<Location*> _srcs;
        const Type* _result;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
            CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        JoinInsn(const vector<Location*>& srcs, const Type* result, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class FieldInsn : public Insn {
        Location* _src;
        u32 _index;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
            CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        FieldInsn(Location* src, u32 index, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };
    
    class CastInsn : public Insn {
        Location* _src;
        const Type* _target;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
            CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        CastInsn(Location* src, const Type* target, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class GotoInsn : public Insn {
        ustring _label;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
                                   CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        GotoInsn(const ustring& label, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class IfEqualInsn : public Insn {
        Location *_lhs, *_rhs;
        ustring _label;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
                                   CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        IfEqualInsn(Location* lhs, Location* rhs, const ustring& label, 
            const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };
    
    class CallInsn : public Insn {
        Location *_operand, *_func;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
                                   CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        CallInsn(Location* operand, Location* _func, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);        
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class RetInsn : public Insn {
        Location *_operand;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen, 
                                   CodeFrame& frame) override;
    public:
        static const InsnClass CLASS;
        RetInsn(Location* operand, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class MovInsn : public Insn {
        Location *_dst, *_src;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
                                   CodeFrame& frame) override;
    public:
        static InsnClass CLASS;
        MovInsn(Location* dst, Location* src, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
        Location* dst() const;
        Location*& dst();
        Location* src() const;
        Location*& src();
    };

    class LeaInsn : public Insn {
        Location* _dst;
        ustring _label;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
                                   CodeFrame& frame) override;
    public:
        static InsnClass CLASS;
        LeaInsn(Location* dst, const ustring& label, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class PrintInsn : public Insn {
        Location* _src;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
                                   CodeFrame& frame) override;
    public:
        static InsnClass CLASS;
        PrintInsn(Location* src, const InsnClass* ic = &CLASS);

        virtual void format(stream& io);
        bool liveout(CodeFrame& gen, const set<Location*>& out) override;
        virtual void emitX86(buffer& text, buffer& data) override;
    };

    class Label : public Insn {
        ustring _label;
        bool _global;
    protected:
        virtual Location* lazyValue(CodeGenerator& gen,
                                   CodeFrame& frame) override;
    public:
        static InsnClass CLASS;
        Label(const ustring& label, bool global, 
            const InsnClass* ic = &CLASS);
        const ustring& label() const;
        virtual void format(stream& io);
        virtual void emitX86(buffer& text, buffer& data) override;
    };
}

void print(stream& s, basil::Location* loc);
void print(basil::Location* loc);

#endif