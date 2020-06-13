#ifndef BASIL_VALUE_H
#define BASIL_VALUE_H

#include "defs.h"
#include "vec.h"
#include "hash.h"
#include "utf8.h"
#include "ir.h"
#include "meta.h"

namespace basil {
    enum Storage {
        STORAGE_GLOBAL, STORAGE_LOCAL, 
        STORAGE_ARGUMENT, STORAGE_CAPTURE
    };

    class Explorer {
    public:
        virtual void visit(Value* value) = 0;
    };

    class Stack {
        ustring _name;
        Stack* _parent;
        vector<Value*> values;
        vector<Stack*> _children;
    public:
        using builtin_t = Value* (*)(const Value*);
        class Entry {
            const Type* _type;
            Meta _value;
            Value* _meta;
            builtin_t _builtin;
            Location* _loc;
            bool _reassigned;
            Storage _storage;
        public:
            Entry(const Type* type = nullptr, builtin_t builtin = nullptr);
            Entry(const Type* type, Value* meta, 
                  builtin_t builtin = nullptr);
            Entry(const Type* type, const Meta& value, 
                  builtin_t builtin = nullptr);
            const Type* type() const;
            builtin_t builtin() const;
            const Meta& value() const;
            Meta& value();
            Value* meta() const;
            Location* loc() const;
            const Type*& type();
            builtin_t& builtin();
            Value*& meta();
            Location* & loc();
            Storage storage() const;
            Storage& storage();
            bool reassigned() const;
            void reassign();
        };
    
    private:
        map<ustring, Entry>* table;

        // type methods
        using tmscope_t = map<pair<const Type*, const Type*>, Entry>;
        using tmcache_t = map<const Type*, set<pair<const Type*, const Type*>>>;
        tmscope_t* tmethods;
        tmcache_t* tmcache;

        u32 _depth;

        const Type* tryApply(const Type* func, Value* arg,
                             u32 line, u32 column);
        const Type* tryApply(Value* func, Value* arg);
        Entry* tryInteract(Value* first, Value* second);
        Value* apply(Value* func, const Type* ft, Value* arg);
    public:
        Stack(Stack* parent = nullptr, bool scope = false);
        ~Stack();
        Stack(const Stack& other);
        Stack& operator=(const Stack& other);

        void detachTo(Stack& s);
        const Value* top() const;
        Value* top();
        const Value* const* begin() const;
        Value** begin();
        const Value* const* end() const;
        Value** end();
        bool expectsMeta();
        void push(Value* v);
        Value* pop();
        bool hasScope() const;
        const Entry* operator[](const ustring& name) const;
        Entry* operator[](const ustring& name);
        const Stack* findenv(const ustring& name) const;
        Stack* findenv(const ustring& name);
        Entry* interactOf(const Type* a, const Type* b);
        void interact(const Type* a, const Type* b, const Meta& f);
        void interact(const Type* a, const Type* b, builtin_t f);
        void bind(const ustring& name, const Type* t);
        void bind(const ustring& name, const Type* t, const Meta& f);
        void bind(const ustring& name, const Type* t, builtin_t b);
        void bind(const ustring& name, const Type* t, Value* value);
        void erase(const ustring& name);
        void clear();
        void copy(Stack& other);
        void copy(vector<Value*>& other);
        u32 size() const;
        const map<ustring, Entry>& scope() const;
        map<ustring, Entry>& scope();
        const map<ustring, Entry>& nearestScope() const;
        map<ustring, Entry>& nearestScope();
        Stack* parent();
        const Stack* parent() const;
        u32 depth() const;
        ustring& name();
        const ustring& name() const;
    };

    class ValueClass {
        const ValueClass* _parent;
    public:
        ValueClass();
        ValueClass(const ValueClass& parent);
        const ValueClass* parent() const;
    };

    class Value {
        u32 _line, _column;
        const ValueClass* _valueclass;
        const Type* _cachetype;
    protected:
        void indent(stream& io, u32 level) const;
        void setType(const Type* t);
        virtual const Type* lazyType(Stack& ctx);
        void updateType(Stack& ctx);
    public:
        static const ValueClass CLASS;

        Value(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual ~Value();
        u32 line() const;
        u32 column() const;
        virtual void format(stream& io, u32 level = 0) const = 0;
        const Type* type(Stack& ctx);
        virtual Meta fold(Stack& ctx);
        virtual bool lvalue(Stack& ctx);
        virtual Stack::Entry* entry(Stack& ctx) const;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame);
        virtual Value* clone(Stack& ctx) const = 0;
        virtual void repr(stream& io) const = 0;
        virtual void explore(Explorer& e);
        virtual bool pure(Stack& ctx) const;

        template<typename T>
        bool is() const {
            const ValueClass* vc = _valueclass;
            while (vc) {
                if (vc == &T::CLASS) return true;
                vc = vc->parent();
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

    class Builtin : public Value {
    public:
        static const ValueClass CLASS;

        Builtin(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) = 0;
        virtual bool canApply(Stack& ctx, Value* arg) const;
    };

    class Void : public Value {
    public:
        static const ValueClass CLASS;

        Void(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class Empty : public Value {
    public:
        static const ValueClass CLASS;

        Empty(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class IntegerConstant : public Value {
        i64 _value;
    public:
        static const ValueClass CLASS;

        IntegerConstant(i64 value, u32 line, u32 column, 
                        const ValueClass* vc = &CLASS);
        i64 value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class RationalConstant : public Value {
        double _value;
    public:
        static const ValueClass CLASS;

        RationalConstant(double value, u32 line, u32 column, 
                        const ValueClass* vc = &CLASS);
        double value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class StringConstant : public Value {
        ustring _value;
    public:
        static const ValueClass CLASS;

        StringConstant(const ustring& value, u32 line, u32 column, 
                        const ValueClass* vc = &CLASS);
        const ustring& value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class CharConstant : public Value {
        uchar _value;
    public:
        static const ValueClass CLASS;

        CharConstant(uchar value, u32 line, u32 column, 
                        const ValueClass* vc = &CLASS);
        uchar value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class TypeConstant : public Value {
        const Type* _value;
    public:
        static const ValueClass CLASS;

        TypeConstant(const Type* value, u32 line, u32 column,
                     const ValueClass* vc = &CLASS);
        const Type* value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class BoolConstant : public Value {
        bool _value;
    public:
        static const ValueClass CLASS;

        BoolConstant(bool value, u32 line, u32 column,
                     const ValueClass* vc = &CLASS);
        bool value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class Incomplete : public Value {
        Term* _term;
    public:
        static const ValueClass CLASS;

        Incomplete(Term* term, u32 line, u32 column,
                   const ValueClass* vc = &CLASS);
        Term* term() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };
    
    class Variable : public Value {
        ustring _name;
        virtual const Type* lazyType(Stack& ctx) override;
    public:
        static const ValueClass CLASS;

        Variable(const ustring& name, u32 line, u32 column,
                 const ValueClass* vc = &CLASS);
        const ustring& name() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual bool lvalue(Stack& ctx) override;
        virtual Stack::Entry* entry(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Interaction : public Value {
        Meta _fn;
    public:
        static const ValueClass CLASS;

        Interaction(const Meta& fn, u32 line, u32 column, 
                    const ValueClass* vc = &CLASS);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class Sequence : public Value {
        vector<Value*> _children;
    protected:
        virtual const Type* lazyType(Stack& ctx);
    public:
        static const ValueClass CLASS;

        Sequence(const vector<Value*>& children, u32 line, u32 column,
                 const ValueClass* vc = &CLASS);
        ~Sequence();
        const vector<Value*>& children() const;
        void append(Value* v);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Program : public Value {
        vector<Value*> _children;
    protected:
        virtual const Type* lazyType(Stack& ctx);
    public:
        static const ValueClass CLASS;

        Program(const vector<Value*>& children, u32 line, u32 column,
                 const ValueClass* vc = &CLASS);
        ~Program();
        const vector<Value*>& children() const;
        void append(Value* v);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Quote : public Builtin {
        Term* _term;
    public:
        static const ValueClass CLASS;

        Quote(u32 line, u32 column, const ValueClass* vc = &CLASS);
        Quote(Term* term, u32 line, u32 column,
              const ValueClass* vc = &CLASS);
        Term* term() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual bool canApply(Stack& ctx, Value* v) const override;
        virtual Value* apply(Stack& ctx, Value* v) override;
        virtual void repr(stream& io) const override;
    };

    class Lambda : public Builtin {
        Stack* _ctx, *_bodyscope;
        Value *_body, *_match;
        ustring _name;
        ustring _label;
        vector<ustring> _alts;
        map<ustring, Stack::Entry> _captures;
        map<const Type*, Lambda*> insts;

        bool _inlined;
    protected:
        virtual const Type* lazyType(Stack& ctx) override;
    public:
        static const ValueClass CLASS;

        Lambda(u32 line, u32 column, 
               const ValueClass* vc = &CLASS);
        ~Lambda();
        Value* match();
        Value* body();
        Stack* scope();
        Stack* self();
        void complete(Stack& ctx);
        void setarg(Stack& ctx, Value* match);
        virtual bool canApply(Stack& ctx, Value* v) const override;
        virtual Value* apply(Stack& ctx, Value* v) override;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual void bindrec(const ustring& name, const Type* type,
                             const Meta& value);
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        bool inlined() const;
        Location* genInline(Stack& ctx, Location* arg, CodeGenerator& gen, CodeFrame& frame);
        const ustring& label() const;
        void addAltLabel(const ustring& label);
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        void instantiate(const Type* t, Lambda* l);
        Lambda* instance(const Type* t);
    };
    
    Lambda* instantiate(Stack& callctx, Lambda* l, const Type* a);

    class Call : public Value {
        Value *_func, *_arg;
        const Type* desiredfn;
        Value* inst;
        bool recursiveMacro;
    protected:
        virtual const Type* lazyType(Stack& ctx) override;
    public:
        static const ValueClass CLASS;

        Call(Value* func, const Type* desired, u32 line, u32 column,
             const ValueClass* vc = &CLASS);
        Call(Value* func, const Type* desired, Value* arg, u32 line, u32 column,
             const ValueClass* vc = &CLASS);
        ~Call();

        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class BinaryOp : public Builtin {
        const char* _opname;
    protected:
        Value *lhs, *rhs;
    public:
        static const ValueClass CLASS;

        BinaryOp(const char* opname, u32 line, u32 column, 
                 const ValueClass* vc = &CLASS);
        ~BinaryOp();
        const Value* left() const;
        const Value* right() const;
        Value* left();
        Value* right();
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual bool canApply(Stack& ctx, Value* arg) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class UnaryOp : public Builtin {
        const char* _opname;
    protected:
        Value *_operand;
    public:
        static const ValueClass CLASS;

        UnaryOp(const char* opname, u32 line, u32 column, 
                 const ValueClass* vc = &CLASS);
        ~UnaryOp();
        const Value* operand() const;
        Value* operand();
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual bool canApply(Stack& ctx, Value* arg) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class BinaryMath : public BinaryOp {
        static const Type *_BASE_TYPE, *_PARTIAL_INT, 
            *_PARTIAL_DOUBLE;
    public:
        static const Type *BASE_TYPE();
        static const Type *PARTIAL_INT_TYPE(), 
            *PARTIAL_DOUBLE_TYPE();
        static const ValueClass CLASS;
    
        BinaryMath(const char* opname, u32 line, u32 column,
                   const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
    };
    
    class Add : public BinaryMath {
        static const Type *_BASE_TYPE, *_PARTIAL_STRING;
    public:
        static const Type *BASE_TYPE();
        static const ValueClass CLASS;

        Add(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };
    
    class Subtract : public BinaryMath {
    public:
        static const ValueClass CLASS;

        Subtract(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };
    
    class Multiply : public BinaryMath {
    public:
        static const ValueClass CLASS;

        Multiply(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };
    
    class Divide : public BinaryMath {
    public:
        static const ValueClass CLASS;

        Divide(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };
    
    class Modulus : public BinaryMath {
    public:
        static const ValueClass CLASS;

        Modulus(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class BinaryLogic : public BinaryOp {
        static const Type *_BASE_TYPE, *_PARTIAL_BOOL;
    public:
        static const Type *BASE_TYPE();
        static const Type *PARTIAL_BOOL_TYPE();
        static const ValueClass CLASS;
    
        BinaryLogic(const char* opname, u32 line, u32 column,
                    const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
    };

    class And : public BinaryLogic {
    public:
        static const ValueClass CLASS;

        And(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class Or : public BinaryLogic {
    public:
        static const ValueClass CLASS;

        Or(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class Xor : public BinaryLogic {
    public:
        static const ValueClass CLASS;

        Xor(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class Not : public UnaryOp {
    public:
        static const ValueClass CLASS;

        Not(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class BinaryEquality : public BinaryOp {
        static const Type *_BASE_TYPE, *_PARTIAL_INT, 
            *_PARTIAL_DOUBLE, *_PARTIAL_BOOL;
    public:
        static const Type *BASE_TYPE();
        static const Type *PARTIAL_INT_TYPE(),
            *PARTIAL_DOUBLE_TYPE(),
            *PARTIAL_BOOL_TYPE();
        static const ValueClass CLASS;
    
        BinaryEquality(const char* opname, u32 line, u32 column,
                       const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
    };

    class Equal : public BinaryEquality {
    public:
        static const ValueClass CLASS;

        Equal(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
    };

    class Inequal : public BinaryEquality {
    public:
        static const ValueClass CLASS;

        Inequal(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
    };

    class BinaryRelation : public BinaryOp {
        static const Type *_BASE_TYPE, *_PARTIAL_INT, 
            *_PARTIAL_DOUBLE;
    public:
        static const Type *BASE_TYPE();
        static const Type *PARTIAL_INT_TYPE(), 
            *PARTIAL_DOUBLE_TYPE();
        static const ValueClass CLASS;
    
        BinaryRelation(const char* opname, u32 line, u32 column,
                       const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
    };

    class Less : public BinaryRelation {
    public:
        static const ValueClass CLASS;

        Less(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
    };

    class LessEqual : public BinaryRelation {
    public:
        static const ValueClass CLASS;

        LessEqual(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
    };

    class Greater : public BinaryRelation {
    public:
        static const ValueClass CLASS;

        Greater(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
    };

    class GreaterEqual : public BinaryRelation {
    public:
        static const ValueClass CLASS;

        GreaterEqual(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
    };

    class ArrayDef : public Builtin {
        Value *_type, *_dim;
    public:
        static const ValueClass CLASS;

        ArrayDef(u32 line, u32 column, 
                  const ValueClass* vc = &CLASS);
        ~ArrayDef();
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* apply(Stack& ctx, Value* v) override;
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };

    class Array : public Builtin {
        vector<Value*> elts;
    public:
        static const ValueClass CLASS;

        Array(u32 line, u32 column, 
              const ValueClass* vc = &CLASS);
        ~Array();
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* apply(Stack& ctx, Value* v) override;
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual bool lvalue(Stack& ctx) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Index : public Builtin {
        Value *arr, *idx;
    public:
        static const ValueClass CLASS;

        Index(u32 line, u32 column, 
              const ValueClass* vc = &CLASS);
        ~Index();
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* apply(Stack& ctx, Value* v) override;
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual bool lvalue(Stack& ctx) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Range : public BinaryOp {
    public:
        static const ValueClass CLASS;
        Range(u32 line, u32 column, const ValueClass* vc = &CLASS);

        virtual Meta fold(Stack& ctx) override;
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class Repeat : public BinaryOp {
    public:
        static const ValueClass CLASS;
        Repeat(u32 line, u32 column, const ValueClass* vc = &CLASS);

        virtual Meta fold(Stack& ctx) override;
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class Cons : public BinaryOp {
    public:
        static const ValueClass CLASS;
        Cons(u32 line, u32 column, const ValueClass* vc = &CLASS);

        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Meta fold(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class Reference : public UnaryOp {
    public:
        static const ValueClass CLASS;

        Reference(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual bool lvalue(Stack& ctx) override;
    };

    class Join : public BinaryOp {
        static const Type *_BASE_TYPE;
    public:
        static const Type *BASE_TYPE();
        static const ValueClass CLASS;
        Join(u32 line, u32 column, const ValueClass* vc = &CLASS);
        
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Meta fold(Stack& ctx) override;
        virtual bool lvalue(Stack& ctx) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual void repr(stream& io) const override;
    };

    class Intersect : public BinaryOp {
        static const Type *_BASE_TYPE;
        map<Meta, Lambda*> _casecache;
        ustring _label;
        virtual const Type* lazyType(Stack& ctx) override;
    protected:
        void populate(Stack& ctx, map<const Type*, 
                      vector<Value*>>& types);
        void getFunctions(Stack& ctx, vector<Lambda*>& functions);
    public:
        static const Type *BASE_TYPE();
        static const ValueClass CLASS;
        Intersect(u32 line, u32 column, const ValueClass* vc = &CLASS);
        
        virtual Meta fold(Stack& ctx) override;
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual void bindrec(const ustring& name, const Type* type,
                             const Meta& value);
        virtual Lambda* caseFor(Stack& ctx, const Meta& value);
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
    };

    class If : public Builtin {
        Value *cond, *body;
    public:
        static const ValueClass CLASS;
        If(u32 line, u32 column, const ValueClass* vc = &CLASS);
        ~If();

        virtual Meta fold(Stack& ctx) override;
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual bool pure(Stack& ctx) const override;
    };

    class While : public Builtin {
        Value *cond, *body;
    public:
        static const ValueClass CLASS;
        While(u32 line, u32 column, const ValueClass* vc = &CLASS);
        ~While();

        virtual Meta fold(Stack& ctx) override;
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Define : public Builtin {
        Value* _type;
        ustring _name;
    public:
        static const ValueClass CLASS;

        Define(Value* type, const ustring& name, 
               const ValueClass* vc = &CLASS);
        ~Define();
        const ustring& name() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual bool canApply(Stack& ctx, Value* arg) const override;
        virtual bool lvalue(Stack& ctx) override;
        virtual Stack::Entry* entry(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual Meta fold(Stack& ctx) override;
    };

    void bind(Stack& ctx, Value* dst, Value* src);
    void assign(Stack& ctx, Value* dst, Value* src);

    class Autodefine : public Builtin {
        Value* _name;
        Value* _init;
    public:
        static const ValueClass CLASS;

        Autodefine(u32 line, u32 column,
                   const ValueClass* vc = &CLASS);
        ~Autodefine();
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual bool canApply(Stack& ctx, Value* arg) const override;
        virtual bool lvalue(Stack& ctx) override;
        virtual Stack::Entry* entry(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual Meta fold(Stack& ctx) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Assign : public Builtin {
        Value *lhs, *rhs;
    public:
        static const ValueClass CLASS;

        Assign(u32 line, u32 column, const ValueClass* vc = &CLASS);
        ~Assign();
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual bool lvalue(Stack& ctx) override;
        virtual Stack::Entry* entry(Stack& ctx) const override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual Meta fold(Stack& ctx) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Print : public UnaryOp {
    public:
        static const Type* _BASE_TYPE;
        static const Type* BASE_TYPE();
        static const ValueClass CLASS;

        Print(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Typeof : public UnaryOp {
    public:
        static const ValueClass CLASS;

        Typeof(u32 line, u32 column, const ValueClass* vc = &CLASS);
        virtual Value* apply(Stack& ctx, Value* arg) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual Meta fold(Stack& ctx) override;
    };

    class Cast : public Value {
        const Type *_dst; 
        Value* _src;
    public:
        static const ValueClass CLASS;

        Cast(const Type* dst, Value* src,
                     const ValueClass* vc = &CLASS);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual Location* gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual void explore(Explorer& e) override;
        virtual bool lvalue(Stack& ctx) override;
        virtual bool pure(Stack& ctx) const override;
    }; 
    

    class Eval : public Builtin {
    public:
        static const ValueClass CLASS;

        Eval(u32 line, u32 column, 
             const ValueClass* vc = &CLASS);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* apply(Stack& ctx, Value* v) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };    
    
    class MetaEval : public Builtin {
        Meta val;
    public:
        static const ValueClass CLASS;

        MetaEval(u32 line, u32 column, 
             const ValueClass* vc = &CLASS);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* apply(Stack& ctx, Value* v) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
        virtual Meta fold(Stack& ctx) override;
        virtual bool pure(Stack& ctx) const override;
    };

    class Use : public Builtin {
        Value* path;
        Source* src;
        Stack* env;
        ProgramTerm* module;
    public:
        static const ValueClass CLASS;

        Use(u32 line, u32 column, const ValueClass* vc = &CLASS);
        ~Use();
        virtual void format(stream& io, u32 level = 0) const override;
        virtual Value* apply(Stack& ctx, Value* v) override;
        virtual Value* clone(Stack& ctx) const override;
        virtual void repr(stream& io) const override;
    };
}

void print(stream& io, basil::Value* t);
void print(basil::Value* t);

#endif