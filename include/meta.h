#ifndef BASIL_META_H
#define BASIL_META_H

#include "defs.h"
#include "vec.h"
#include "hash.h"
#include "utf8.h"
#include "io.h"

namespace basil {
    extern u64 findSymbol(const ustring& name);
    extern const ustring& findSymbol(u64 name);

    class Meta {
        const Type* _type;
        union {
            i64 i;
            u64 u;
            double d;
            const Type* t;
            bool b;
            Meta* r;
            MetaString* s;
            MetaList* l;
            MetaTuple* tu;
            MetaArray* a;
            MetaBlock* bl;
            MetaUnion* un;
            MetaIntersect* in;
            MetaFunction* f;
            MetaMacro* m;
        } value;

        void free();
        void copy(const Meta& other);
        void assign(const Meta& other);
    public:
        Meta();
        Meta(const Type* type);
        Meta(const Type* type, i64 i);
        Meta(const Type* type, u64 u);
        Meta(const Type* type, double d);
        Meta(const Type* type, const Type* t);
        Meta(const Type* type, bool b);
        Meta(const Type* type, Meta& r);
        Meta(const Type* type, const ustring& s);
        Meta(const Type* type, MetaList* l);
        Meta(const Type* type, MetaTuple* tu);
        Meta(const Type* type, MetaArray* a);
        Meta(const Type* type, MetaBlock* bl);
        Meta(const Type* type, MetaUnion* un);
        Meta(const Type* type, MetaIntersect* in);
        Meta(const Type* type, MetaFunction* f);
        Meta(const Type* type, MetaMacro* m);
        ~Meta();
        Meta(const Meta& other);
        Meta& operator=(const Meta& other);
        const Type* type() const;
        bool isVoid() const;
        bool isInt() const;
        i64 asInt() const;
        i64& asInt();
        bool isUint() const;
        u64 asUint() const;
        u64& asUint();
        bool isFloat() const;
        double asFloat() const;
        double& asFloat();
        bool isType() const;
        const Type* asType() const;
        const Type*& asType();
        bool isBool() const;
        bool asBool() const;
        bool& asBool();
        bool isSymbol() const;
        u64 asSymbol() const;
        u64& asSymbol();
        bool isRef() const;
        const Meta& asRef() const;
        Meta& asRef();
        bool isString() const;
        const ustring& asString() const;
        ustring& asString();
        bool isList() const;
        const MetaList& asList() const;
        MetaList& asList();
        bool isTuple() const;
        const MetaTuple& asTuple() const;
        MetaTuple& asTuple();
        bool isArray() const;
        const MetaArray& asArray() const;
        MetaArray& asArray();
        bool isBlock() const;
        const MetaBlock& asBlock() const;
        MetaBlock& asBlock();
        bool isUnion() const;
        const MetaUnion& asUnion() const;
        MetaUnion& asUnion();
        bool isIntersect() const;
        const MetaIntersect& asIntersect() const;
        MetaIntersect& asIntersect();
        bool isFunction() const;
        const MetaFunction& asFunction() const;
        MetaFunction& asFunction();
        bool isMacro() const;
        const MetaMacro& asMacro() const;
        MetaMacro& asMacro();
        operator bool() const;
        Meta clone() const;
        void format(stream& io) const;
        bool operator==(const Meta& fr) const;
        bool operator!=(const Meta& fr) const;
        ustring toString() const;
        u64 hash() const;
    };

    class MetaRC {
        u32 rc;
    public:
        MetaRC();
        virtual ~MetaRC();
        void inc();
        void dec();
        virtual Meta clone(const Meta& src) const = 0;
    };

    class MetaString : public MetaRC {
        ustring s;
    public:
        MetaString(const ustring& str);
        const ustring& str() const;
        ustring& str();
        Meta clone(const Meta& src) const override;
    };

    class MetaList : public MetaRC {
        Meta v;
        Meta n;
    public:
        MetaList();
        MetaList(Meta val, Meta next);
        const Meta& head() const;
        Meta& head();
        const Meta& tail() const;
        Meta& tail();
        Meta clone(const Meta& src) const override;
    };

    class MetaTuple : public MetaRC {
        vector<Meta> vals;
    public:
        MetaTuple(const vector<Meta>& values);
        const Meta& operator[](u32 i) const;
        Meta& operator[](u32 i);
        const Meta* begin() const;
        const Meta* end() const;
        Meta* begin();
        Meta* end();
        u32 size() const;
        Meta clone(const Meta& src) const override;
    };

    class MetaArray : public MetaRC {
        vector<Meta> vals;
    public:
        MetaArray(const vector<Meta>& values);
        const Meta& operator[](u32 i) const;
        Meta& operator[](u32 i);
        const Meta* begin() const;
        const Meta* end() const;
        Meta* begin();
        Meta* end();
        u32 size() const;
        Meta clone(const Meta& src) const override;
    };

    class MetaBlock : public MetaRC {
        vector<Meta> vals;
    public:
        MetaBlock(const vector<Meta>& values);
        void push(const Meta& elt);
        void cat(const Meta& block);
        Meta slice(u32 start, u32 finish) const;
        const Meta& operator[](u32 i) const;
        Meta& operator[](u32 i);
        const Meta* begin() const;
        const Meta* end() const;
        Meta* begin();
        Meta* end();
        u32 size() const;
        Meta clone(const Meta& src) const override;
    };

    class MetaUnion : public MetaRC {
        Meta real;
    public:
        MetaUnion(const Meta& val);
        bool is(const Type* t) const;
        const Meta& value() const;
        Meta& value();
        Meta clone(const Meta& src) const override;
    };

    class MetaIntersect : public MetaRC {
        vector<Meta> vals;
    public:
        MetaIntersect(const vector<Meta>& values);
        const Meta& as(const Type* t) const;
        Meta& as(const Type* t);
        const Meta* begin() const;
        const Meta* end() const;
        Meta* begin();
        Meta* end();
        u32 size() const;
        Meta clone(const Meta& src) const override;
    };

    class MetaFunction : public MetaRC {
        Value* fn;
        map<ustring, Meta>* _captures;
    public:
        MetaFunction(Value* function);
        MetaFunction(Value* function, const map<ustring, Meta>& captures);
        ~MetaFunction();
        Value* value() const;
        map<ustring, Meta>* captures();
        const map<ustring, Meta>* captures() const;
        Meta clone(const Meta& src) const override;
    };

    class MetaMacro : public MetaRC {
        Value* mac;
    public:
        MetaMacro(Value* macro);
        Value* value() const;
        Meta clone(const Meta& src) const override;
    };

    i64 trunc(i64 n, const Type* dest);
    u64 trunc(u64 n, const Type* dest);
    double toFloat(const Meta& m);
    i64 toInt(const Meta& m);
    u64 toUint(const Meta& m);

    Meta add(const Meta& lhs, const Meta& rhs);
    Meta sub(const Meta& lhs, const Meta& rhs);
    Meta mul(const Meta& lhs, const Meta& rhs);
    Meta div(const Meta& lhs, const Meta& rhs);
    Meta mod(const Meta& lhs, const Meta& rhs);
    Meta andf(const Meta& lhs, const Meta& rhs);
    Meta orf(const Meta& lhs, const Meta& rhs);
    Meta xorf(const Meta& lhs, const Meta& rhs);
    Meta notf(const Meta& operand);
    Meta equal(const Meta& lhs, const Meta& rhs);
    Meta inequal(const Meta& lhs, const Meta& rhs);
    Meta less(const Meta& lhs, const Meta& rhs);
    Meta lessequal(const Meta& lhs, const Meta& rhs);
    Meta greater(const Meta& lhs, const Meta& rhs);
    Meta greaterequal(const Meta& lhs, const Meta& rhs);
    Meta cons(const Meta& lhs, const Meta& rhs);
    Meta join(const Meta& lhs, const Meta& rhs);
    Meta unionf(const Meta& lhs, const Meta& rhs);
    Meta intersect(const Meta& lhs, const Meta& rhs);
    void assign(Meta& lhs, const Meta& rhs);
}

template<>
u64 hash(const basil::Meta& m);

void print(stream& io, const basil::Meta& m);
void print(const basil::Meta& m);

#endif