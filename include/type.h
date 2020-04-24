#ifndef BASIL_TYPE_H
#define BASIL_TYPE_H

#include "defs.h"
#include "utf8.h"
#include "vec.h"
#include "meta.h"
#include <initializer_list>

namespace basil {
    enum ConstraintType {
        NULL_CONSTRAINT = 0, UNKNOWN = 1, EQUALS_VALUE = 2, OF_TYPE = 3
    };
    
    class Constraint {
        ConstraintType _type;
        Meta _value;
        ustring _key;
        Constraint(ConstraintType type);
    public:
        Constraint(Meta value);
        Constraint(const Type* type);
        Constraint();

        const static Constraint NONE;

        ConstraintType type() const;
        Meta value() const;
        bool conflictsWith(const Constraint& other) const;
        bool precedes(const Constraint& other) const;
        bool matches(Meta value) const;
        const ustring& key() const;
        operator bool() const;
    };

    class TypeClass {
        const TypeClass* _parent;
    public:
        TypeClass();
        TypeClass(const TypeClass& parent);
        const TypeClass* parent() const;
    };

    class Type {
        const TypeClass* _typeclass;
        static u32 nextid;
    protected:
        ustring _key;
        u32 _size;
        u32 _id;
    public:
        static const TypeClass CLASS;

        Type(const ustring& key, u32 size, const TypeClass* tc = &CLASS);
        
        u32 size() const;
        virtual bool implicitly(const Type* other) const;
        virtual bool explicitly(const Type* other) const;
        virtual bool conflictsWith(const Type* other) const;
        const ustring& key() const;
        virtual void format(stream& io) const;
        u32 id() const;

        template<typename T>
        bool is() const {
            const TypeClass* tc = _typeclass;
            while (tc) {
                if (tc == &T::CLASS) return true;
                tc = tc->parent();
            }
            return false;
        }

        template<typename T>
        T* as() {
            return (T*)this;
        }

        template<typename T>
        const T* as() const {
            return (const T*)this;
        }
    };

    class NumericType : public Type {
        bool _floating, _signed;
    public:
        static const TypeClass CLASS;

        NumericType(u32 size, bool floating, bool sign,
                    const TypeClass* tc = &CLASS);
        bool floating() const;
        bool isSigned() const;
        virtual bool implicitly(const Type* other) const override;
        virtual bool explicitly(const Type* other) const override;
    };

    class TupleType : public Type {
        vector<const Type*> _members;
        vector<u32> _offsets;
    public:
        static const TypeClass CLASS;

        TupleType(const vector<const Type*>& members,
                  const TypeClass* tc = &CLASS);
        const vector<const Type*>& members() const;
        const Type* member(u32 i) const;
        u32 offset(u32 i) const;
        u32 count() const;
        virtual bool implicitly(const Type* other) const override;
        virtual bool explicitly(const Type* other) const override;
        virtual void format(stream& io) const override;
    };

    class UnionType : public Type {
        set<const Type*> _members;
    public:
        static const TypeClass CLASS;

        UnionType(const set<const Type*>& members,
                  const TypeClass* tc = &CLASS);
        const set<const Type*>& members() const;
        bool has(const Type* t) const;
        virtual bool implicitly(const Type* other) const override;
        virtual bool explicitly(const Type* other) const override;
        virtual void format(stream& io) const override;
    };

    class IntersectionType : public Type {
        set<const Type*> _members;
    public:
        static const TypeClass CLASS;

        IntersectionType(const set<const Type*>& members,
                         const TypeClass* tc = &CLASS);
        const set<const Type*>& members() const;
        bool has(const Type* t) const;
        virtual bool conflictsWith(const Type* other) const override;
        virtual bool implicitly(const Type* other) const override;
        virtual bool explicitly(const Type* other) const override;
        virtual void format(stream& io) const override;
    };

    class ListType : public Type {
        const Type* _element;
    public:
        static const TypeClass CLASS;

        ListType(const Type* element, const TypeClass* tc = &CLASS);
        const Type* element() const;
        virtual bool implicitly(const Type* other) const override;
        virtual bool explicitly(const Type* other) const override;
        virtual void format(stream& io) const override;
    };

    class EmptyType : public Type {
    public:
        static const TypeClass CLASS;

        EmptyType(const TypeClass* tc = &CLASS);
        virtual bool implicitly(const Type* other) const override;
        virtual bool explicitly(const Type* other) const override;
        virtual void format(stream& io) const override;
    };

    class MacroType : public Type {
        const Type *_arg;
        vector<Constraint> _cons;
        bool _quoting;
    public:
        static const TypeClass CLASS;

        MacroType(const Type* arg, bool quoting,
                  const vector<Constraint>& cons = {},
                  const TypeClass* tc = &CLASS);
        MacroType(const Type* arg, 
                  const vector<Constraint>& cons = {},
                  const TypeClass* tc = &CLASS);
        const Type* arg() const;
        bool quoting() const;
        const vector<Constraint>& constraints() const;
        virtual bool conflictsWith(const Type* other) const override;
        virtual bool implicitly(const Type* other) const override;
        virtual bool explicitly(const Type* other) const override;
        virtual void format(stream& io) const override;
        virtual Constraint matches(Meta fr) const;
    };

    class FunctionType : public Type {
        const Type *_arg, *_ret;
        vector<Constraint> _cons;
        bool _quoting;
    public:
        static const TypeClass CLASS;

        FunctionType(const Type* arg, const Type* ret, bool quoting,
                     const vector<Constraint>& cons = {}, 
                     const TypeClass* tc = &CLASS);
        FunctionType(const Type* arg, const Type* ret,
                     const vector<Constraint>& cons = {}, 
                     const TypeClass* tc = &CLASS);
        const Type* arg() const;
        const Type* ret() const;
        bool quoting() const;
        const vector<Constraint>& constraints() const;
        virtual bool conflictsWith(const Type* other) const override;
        virtual bool implicitly(const Type* other) const override;
        virtual bool explicitly(const Type* other) const override;
        virtual void format(stream& io) const override;
        virtual Constraint matches(Meta fr) const;
    };

    Constraint maxMatch(const vector<Constraint>& cons, Meta fr);

    extern map<ustring, const Type*> typemap;

    template<typename T, typename... Args>
    const T* find(Args... args) {
        T t(args...);
        auto it = typemap.find(t.key());
        if (it == typemap.end()) {
            const T* ptr = new T(t);
            typemap.insert({ t.key(), ptr });
            return ptr;
        }
        return it->second->template as<T>();
    }

    template<typename T, typename U, typename... Args>
    const T* find(const std::initializer_list<U>& ini, Args... args) {
        return find<T>(vector<U>(ini), args...);
    }

    const Type* join(const Type* a, const Type* b);

    extern const Type *I8, *I16, *I32, *I64, *U8, *U16, *U32, *U64,
                      *FLOAT, *DOUBLE, *BOOL, *TYPE, *ERROR, 
                      *VOID, *ANY, *STRING, *CHAR, *EMPTY,
                      *SYMBOL;
}

void print(stream& io, const basil::Type* t);
void print(const basil::Type* t);

#endif