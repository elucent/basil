#include "meta.h"
#include "type.h"
#include "io.h"
#include "errors.h"
#include "value.h"

namespace basil {
    map<u64, ustring> symbolnames;
    map<ustring, u64> symbolids;
    u64 next = 0;

    u64 findSymbol(const ustring& name) {
        auto it = symbolids.find(name);
        if (it == symbolids.end()) {
            u64 id = next ++;
            symbolnames[symbolids[name] = id] = name;
            return id;
        }
        return it->second;
    }

    const ustring& findSymbol(u64 id) {
        auto it = symbolnames.find(id);
        if (it == symbolnames.end()) {
            return "";
        }
        return it->second;
    }

    // Meta

    void Meta::free() {
        if (isList()) value.l->dec();
        else if (isString()) value.s->dec();
        else if (isArray()) value.a->dec();
        else if (isTuple()) value.tu->dec();
        else if (isUnion()) value.un->dec();
        else if (isIntersect()) value.in->dec();
        else if (isFunction()) value.f->dec();
    }

    void Meta::copy(const Meta& other) {
        _type = other._type;
        if (!_type || isVoid()) return;
        else if (isInt()) value.i = other.value.i;
        else if (isFloat()) value.d = other.value.d;
        else if (isType()) value.t = other.value.t;
        else if (isBool()) value.b = other.value.b;
        else if (isSymbol()) value.i = other.value.i;
        else if (isRef()) value.r = other.value.r;
        else if (isString()) value.s = other.value.s, value.s->inc();
        else if (isList()) value.l = other.value.l, value.l->inc();
        else if (isTuple()) value.tu = other.value.tu, value.tu->inc();
        else if (isArray()) value.a = other.value.a, value.a->inc();
        else if (isUnion()) value.un = other.value.un, value.un->inc();
        else if (isIntersect()) value.in = other.value.in, value.in->inc();
        else if (isFunction()) value.f = other.value.f, value.f->inc();
    }

    void Meta::assign(const Meta& other) {
        if (!_type) return copy(other);
        auto prev = value;
        _type = other._type;
        value.s = nullptr;
        if (!_type || isVoid()) return;
        else if (isInt()) value.i = other.value.i;
        else if (isFloat()) value.d = other.value.d;
        else if (isType()) value.t = other.value.t;
        else if (isBool()) value.b = other.value.b;
        else if (isSymbol()) value.i = other.value.i;
        else if (isRef()) value.r = other.value.r;
        else if (isString()) value.s = other.value.s, value.s->inc(), prev.s ? prev.s->dec() : void();
        else if (isList()) value.l = other.value.l, value.l->inc(), prev.l ? prev.l->dec() : void();
        else if (isTuple()) value.tu = other.value.tu, value.tu->inc(), prev.tu ? prev.tu->dec() : void();
        else if (isArray()) value.a = other.value.a, value.a->inc(), prev.a ? prev.a->dec() : void();
        else if (isUnion()) value.un = other.value.un, value.un->inc(), prev.un ? prev.un->dec() : void();
        else if (isIntersect()) value.in = other.value.in, value.in->inc(), prev.in ? prev.in->dec() : void();
        else if (isFunction()) value.f = other.value.f, value.f->inc(), prev.f ? prev.f->dec() : void();
    }

    Meta::Meta(): _type(nullptr) {
        //
    }

    Meta::Meta(const Type* type): _type(type) {
        //
    }

    Meta::Meta(const Type* type, i64 i): Meta(type) {
        value.i = i;
    }

    Meta::Meta(const Type* type, double d): Meta(type) {
        value.d = d;
    }

    Meta::Meta(const Type* type, const Type* t): Meta(type) {
        value.t = t;
    }

    Meta::Meta(const Type* type, bool b): Meta(type) {
        value.b = b;
    }

    Meta::Meta(const Type* type, Meta& r): Meta(type) {
        value.r = &r;
    }

    Meta::Meta(const Type* type, const ustring& s): Meta(type) {
        if (type == STRING)
            value.s = new MetaString(s);
        else if (type == SYMBOL)
            value.i = findSymbol(s);
    }

    Meta::Meta(const Type* type, MetaList* l): Meta(type) {
        value.l = l;
    }

    Meta::Meta(const Type* type, MetaTuple* tu): Meta(type) {
        value.tu = tu;
    }

    Meta::Meta(const Type* type, MetaArray* a): Meta(type) {
        value.a = a;
    }

    Meta::Meta(const Type* type, MetaUnion* un): Meta(type) {
        value.un = un;
    }

    Meta::Meta(const Type* type, MetaIntersect* in): Meta(type) {
        value.in = in;
    }

    Meta::Meta(const Type* type, MetaFunction* f): Meta(type) {
        value.f = f;
    }

    Meta::~Meta() {
        free();
    }

    Meta::Meta(const Meta& other) {
        copy(other);
    }

    Meta& Meta::operator=(const Meta& other) {
        if (this != &other) {
            assign(other);
        }
        return *this;
    }

    const Type* Meta::type() const {
        return _type;
    }

    bool Meta::isVoid() const {
        return _type == VOID;
    }

    bool Meta::isInt() const {
        if (!_type) return false;
        return _type->is<NumericType>() 
            && !_type->as<NumericType>()->floating();
    }

    i64 Meta::asInt() const {
        return value.i;
    }

    i64& Meta::asInt() {
        return value.i;
    }

    bool Meta::isFloat() const {
        if (!_type) return false;
        return _type->is<NumericType>() 
            && _type->as<NumericType>()->floating();
    }

    double Meta::asFloat() const {
        return value.d;
    }

    double& Meta::asFloat() {
        return value.d;
    }

    bool Meta::isType() const {
        return _type == TYPE;
    }

    const Type* Meta::asType() const {
        return value.t;
    }

    const Type*& Meta::asType() {
        return value.t;
    }

    bool Meta::isBool() const {
        return _type == BOOL;
    }

    bool Meta::asBool() const {
        return value.b;
    }

    bool& Meta::asBool() {
        return value.b;
    }

    bool Meta::isSymbol() const {
        return _type == SYMBOL;
    }

    i64 Meta::asSymbol() const {
        return value.i;
    }

    i64& Meta::asSymbol() {
        return value.i;
    }

    bool Meta::isRef() const {
        if (!_type) return false;
        return _type->is<ReferenceType>();
    }

    const Meta& Meta::asRef() const {
        return *value.r;
    }

    Meta& Meta::asRef() {
        return *value.r;
    }

    bool Meta::isString() const {
        return _type == STRING;
    }

    const ustring& Meta::asString() const {
        return value.s->str();
    }

    ustring& Meta::asString() {
        return value.s->str();
    }

    bool Meta::isList() const {
        if (!_type) return false;
        return _type->is<ListType>();
    }

    const MetaList& Meta::asList() const {
        return *value.l;
    }

    MetaList& Meta::asList() {
        return *value.l;
    }

    bool Meta::isTuple() const {
        if (!_type) return false;
        return _type->is<TupleType>();
    }

    const MetaTuple& Meta::asTuple() const {
        return *value.tu;
    }

    MetaTuple& Meta::asTuple() {
        return *value.tu;
    }

    bool Meta::isArray() const {
        if (!_type) return false;
        return _type->is<ArrayType>();
    }

    const MetaArray& Meta::asArray() const {
        return *value.a;
    }

    MetaArray& Meta::asArray() {
        return *value.a;
    }

    bool Meta::isUnion() const {
        if (!_type) return false;
        return _type->is<UnionType>();
    }

    const MetaUnion& Meta::asUnion() const {
        return *value.un;
    }

    MetaUnion& Meta::asUnion() {
        return *value.un;
    }

    bool Meta::isIntersect() const {
        if (!_type) return false;
        return _type->is<IntersectionType>();
    }

    const MetaIntersect& Meta::asIntersect() const {
        return *value.in;
    }

    MetaIntersect& Meta::asIntersect() {
        return *value.in;
    }

    bool Meta::isFunction() const {
        if (!_type) return false;
        return _type->is<FunctionType>();
    }

    const MetaFunction& Meta::asFunction() const {
        return *value.f;
    }

    MetaFunction& Meta::asFunction() {
        return *value.f;
    }

    Meta::operator bool() const {
        return _type;
    }

    Meta Meta::clone() const {
        if (isList()) return value.l->clone(*this);
        else if (isString()) return value.s->clone(*this);
        else if (isArray()) return value.a->clone(*this);
        else if (isTuple()) return value.tu->clone(*this);
        else if (isUnion()) return value.un->clone(*this);
        else if (isIntersect()) return value.in->clone(*this);
        else if (isFunction()) return value.f->clone(*this);
        return *this;
    }

    void Meta::format(stream& io) const {
        if (!_type) print(io, "<null>");
        else if (isVoid()) print(io, "()");
        else if (isInt()) print(io, asInt());
        else if (isFloat()) print(io, asFloat());
        else if (isType()) print(io, asType());
        else if (isBool()) print(io, asBool());
        else if (isSymbol()) print(io, findSymbol(asSymbol()));
        else if (isString()) print(io, asString());
        else if (isRef()) print(io, "~", asRef());
        else if (isList()) 
            print(io, "(", asList().head(), " :: ", asList().tail(), ")");
        else if (isTuple()) {
            print(io, "(");
            for (u32 i = 0; i < asTuple().size(); i ++) {
                print(io, i != 0 ? ", " : "", asTuple()[i]);
            }
            print(io, ")");
        }
        else if (isArray()) {
            print(io, "[");
            for (u32 i = 0; i < asArray().size(); i ++) {
                print(io, i != 0 ? " " : "", asArray()[i]);
            }
            print(io, "]");
        }
        else if (isUnion())
            print(io, asUnion().value());
        else if (isIntersect()) {
            print(io, "(");
            for (u32 i = 0; i < asTuple().size() - 1; i ++) {
                print(io, i != 0 ? " & " : "", asTuple()[i]);
            }
            print(io, ")");
        }
        else if (isFunction()) print(io, "<function: ", asFunction().value(), ">");
    }

    bool Meta::operator==(const Meta& m) const {
        if (_type != m._type) return false;
        if (!_type) return true;
        else if (isVoid()) return true;
        else if (isInt()) return asInt() == m.asInt();
        else if (isFloat()) return asFloat() == m.asFloat();
        else if (isType()) return asType() == m.asType();
        else if (isBool()) return asBool() == m.asBool();
        else if (isSymbol()) return asSymbol() == m.asSymbol();
        else if (isString()) return asString() == m.asString();
        else if (isList()) 
            return asList().head() == m.asList().head()
                && asList().tail() == m.asList().tail();
        else if (isTuple()) {
            for (u32 i = 0; i < asTuple().size(); i ++) {
                if (asTuple()[i] != m.asTuple()[i]) return false;
            }
        }
        else if (isArray()) {
            for (u32 i = 0; i < asArray().size(); i ++) {
                if (asArray()[i] != m.asArray()[i]) return false;
            }
        }
        else if (isUnion())
            return m.asUnion().value() == asUnion().value();
        else if (isIntersect()) {
            for (const Type* t : _type->as<IntersectionType>()->members()) {
                if (asIntersect().as(t) != m.asIntersect().as(t)) return false;
            }
        }
        else if (isFunction()) 
            return asFunction().value() == m.asFunction().value();
        return true;
    }

    bool Meta::operator!=(const Meta& m) const {
        return !(*this == m);
    }

    u64 Meta::hash() const {
        u64 h = ::hash(_type);
        if (!_type) return h;
        else if (isVoid()) return h;
        else if (isInt()) return h ^ ::hash(asInt());
        else if (isFloat()) return h ^ ::hash(asFloat());
        else if (isType()) return h ^ ::hash(asType());
        else if (isBool()) return h ^ ::hash(asBool());
        else if (isSymbol()) return h ^ ::hash(asSymbol());
        else if (isString()) return h ^ ::hash(asString());
        else if (isList()) 
            return h ^ asList().head().hash() ^ asList().tail().hash();
        else if (isTuple()) {
            for (u32 i = 0; i < asTuple().size(); i ++) {
                h ^= asTuple()[i].hash();
            }
            return h;
        }
        else if (isArray()) {
            for (u32 i = 0; i < asArray().size(); i ++) {
                h ^= asArray()[i].hash();
            }
            return h;
        }
        else if (isUnion())
            return h ^ asUnion().value().hash();
        else if (isIntersect()) {
            for (const Type* t : _type->as<IntersectionType>()->members()) {
                h ^= asIntersect().as(t);
            }
            return h;
        }
        else if (isFunction()) 
            return h ^ ::hash(asFunction().value());
        return h;
    }

    ustring Meta::toString() const {
        buffer b;
        format(b);
        ustring s;
        fread(b, s);
        return s;
    }

    // MetaRC

    MetaRC::MetaRC(): rc(1) {
        //
    }

    MetaRC::~MetaRC() {
        //
    }
     
    void MetaRC::inc() {
        rc ++;
    }

    void MetaRC::dec() {
        rc --;
        if (rc == 0) delete this;
    }

    // MetaString

    MetaString::MetaString(const ustring& str):
        s(str) {
        //
    }

    const ustring& MetaString::str() const {
        return s;
    }

    ustring& MetaString::str() {
        return s;
    }

    Meta MetaString::clone(const Meta& src) const {
        return Meta(src.type(), str());
    }

    // MetaList

    MetaList::MetaList() {
        // initializes val and next to null meta
    }

    MetaList::MetaList(Meta val, Meta next):
        v(val), n(next) {
        //
    }

    const Meta& MetaList::head() const {
        return v;
    }

    Meta& MetaList::head() {
        return v;
    }

    const Meta& MetaList::tail() const {
        return n;
    }

    Meta& MetaList::tail() {
        return n;
    }

    Meta MetaList::clone(const Meta& src) const {
        return Meta(src.type(), new MetaList(v.clone(), n.clone()));
    }

    // MetaTuple

    MetaTuple::MetaTuple(const vector<Meta>& values): vals(values) {
        //
    }

    const Meta& MetaTuple::operator[](u32 i) const {
        return vals[i];
    }

    Meta& MetaTuple::operator[](u32 i) {
        return vals[i];
    }

    const Meta* MetaTuple::begin() const {
        return vals.begin();
    }

    const Meta* MetaTuple::end() const {
        return vals.end();
    }

    Meta* MetaTuple::begin() {
        return vals.begin();
    }

    Meta* MetaTuple::end() {
        return vals.end();
    }

    u32 MetaTuple::size() const {
        return vals.size();
    }

    Meta MetaTuple::clone(const Meta& src) const {
        vector<Meta> copies;
        for (const Meta& m : *this) copies.push(m.clone());
        return Meta(src.type(), new MetaTuple(copies));
    }

    // MetaArray

    MetaArray::MetaArray(const vector<Meta>& values): vals(values) {
        //
    }

    const Meta& MetaArray::operator[](u32 i) const {
        return vals[i];
    }

    Meta& MetaArray::operator[](u32 i) {
        return vals[i];
    }

    const Meta* MetaArray::begin() const {
        return vals.begin();
    }

    const Meta* MetaArray::end() const {
        return vals.end();
    }

    Meta* MetaArray::begin() {
        return vals.begin();
    }

    Meta* MetaArray::end() {
        return vals.end();
    }

    u32 MetaArray::size() const {
        return vals.size();
    }

    Meta MetaArray::clone(const Meta& src) const {
        vector<Meta> copies;
        for (const Meta& m : *this) copies.push(m.clone());
        return Meta(src.type(), new MetaArray(copies));
    }

    // MetaUnion

    MetaUnion::MetaUnion(const Meta& val): real(val) {
        //
    }

    bool MetaUnion::is(const Type* t) const {
        return real.type()->explicitly(t);
    }

    const Meta& MetaUnion::value() const {
        return real;
    }

    Meta& MetaUnion::value() {
        return real;
    }

    Meta MetaUnion::clone(const Meta& src) const {
        return Meta(src.type(), new MetaUnion(real));
    }

    // MetaIntersect

    MetaIntersect::MetaIntersect(const vector<Meta>& values): vals(values) {
        vals.push(Meta());
    }

    const Meta& MetaIntersect::as(const Type* t) const {
        for (const Meta& m : vals) if (m.type() == t) return m;
        return vals.back();
    }

    Meta& MetaIntersect::as(const Type* t) {
        for (Meta& m : vals) if (m.type() == t) return m;
        return vals.back();
    }

    const Meta* MetaIntersect::begin() const {
        return vals.begin();
    }

    const Meta* MetaIntersect::end() const {
        return begin() + size();
    }

    Meta* MetaIntersect::begin() {
        return vals.begin();
    }

    Meta* MetaIntersect::end() {
        return begin() + size();
    }

    u32 MetaIntersect::size() const {
        return size() - 1;
    }

    Meta MetaIntersect::clone(const Meta& src) const {
        vector<Meta> copies;
        for (const Meta& m : vals) if (m) copies.push(m.clone());
        return Meta(src.type(), new MetaIntersect(copies));
    }

    // MetaFunction

    MetaFunction::MetaFunction(Value* function): 
        fn(function), _captures(nullptr) {
        //
    }

    MetaFunction::MetaFunction(Value* function, const map<ustring, Meta>& captures):   
        fn(function), _captures(new map<ustring, Meta>(captures)) {
        //
    }

    MetaFunction::~MetaFunction() {
        if (_captures) delete _captures;
    }

    Value* MetaFunction::value() const {
        return fn;
    }

    map<ustring, Meta>* MetaFunction::captures() {
        return _captures;
    }

    const map<ustring, Meta>* MetaFunction::captures() const {
        return _captures;
    }

    Meta MetaFunction::clone(const Meta& src) const {
        return Meta(src.type(), new MetaFunction(fn));
    }

    // Meta Ops

    i64 trunc(i64 n, const Type* dest) {
        switch (dest->size()) {
            case 1:
                return i8(n);
            case 2:
                return i16(n);
            case 4:
                return i32(n);
            default:
                return n;
        }
    }

    u64 trunc(u64 n, const Type* dest) {
        switch(dest->size()) {
            case 1:
                return u8(n);
            case 2:
                return u16(n);
            case 4:
                return u32(n);
            default:
                return n;
        }
    }

    double toFloat(const Meta& m) {
        if (m.isFloat()) return m.asFloat();
        else if (m.isInt()) return m.asInt();
        return 0;
    }

    i64 toInt(const Meta& m) {
        if (m.isInt()) return m.asInt();
        else if (m.isFloat()) return i64(m.asFloat());
        return 0;
    }

    double fmod(double l, double r) {
        i64 denom = l / r < 0 ? i64(l / r) - 1 : i64(l / r);
        return l - r * denom;
    }

    Meta add(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(dst, toFloat(lhs) + toFloat(rhs));
        else if (dst->is<NumericType>())
            return Meta(dst, trunc(toInt(lhs) + toInt(rhs), dst));
        else if (dst == STRING)
            return Meta(dst, lhs.asString() + rhs.asString());
        return Meta();
    }

    Meta sub(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(dst, toFloat(lhs) - toFloat(rhs));
        else if (dst->is<NumericType>())
            return Meta(dst, trunc(toInt(lhs) - toInt(rhs), dst));
        return Meta();
    }

    Meta mul(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(dst, toFloat(lhs) * toFloat(rhs));
        else if (dst->is<NumericType>())
            return Meta(dst, trunc(toInt(lhs) * toInt(rhs), dst));
        return Meta();
    }

    Meta div(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(dst, toFloat(lhs) / toFloat(rhs));
        else if (dst->is<NumericType>())
            return Meta(dst, trunc(toInt(lhs) / toInt(rhs), dst));
        return Meta();
    }

    Meta mod(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(dst, fmod(toFloat(lhs), toFloat(rhs)));
        else if (dst->is<NumericType>())
            return Meta(dst, trunc(toInt(lhs) % toInt(rhs), dst));
        return Meta();
    }

    Meta andf(const Meta& lhs, const Meta& rhs) {
        if (!lhs.isBool() || !rhs.isBool()) return Meta();
        return Meta(BOOL, lhs.asBool() && rhs.asBool());
    }

    Meta orf(const Meta& lhs, const Meta& rhs) {
        if (!lhs.isBool() || !rhs.isBool()) return Meta();
        return Meta(BOOL, lhs.asBool() || rhs.asBool());
    }

    Meta xorf(const Meta& lhs, const Meta& rhs) {
        if (!lhs.isBool() || !rhs.isBool()) return Meta();
        return Meta(BOOL, bool(lhs.asBool() ^ rhs.asBool()));
    }

    Meta notf(const Meta& operand) {
        if (!operand.isBool()) return Meta();
        return Meta(BOOL, !operand.asBool());
    }

    Meta equal(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        return Meta(BOOL, lhs == rhs);
    }

    Meta inequal(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        return Meta(BOOL, lhs != rhs);
    }

    Meta less(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(BOOL, toFloat(lhs) < toFloat(rhs));
        else if (dst->is<NumericType>())
            return Meta(BOOL, toInt(lhs) < toInt(rhs));
        else if (dst == STRING)
            return Meta(BOOL, lhs.asString() < rhs.asString());
        return Meta();
    }

    Meta lessequal(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(BOOL, toFloat(lhs) <= toFloat(rhs));
        else if (dst->is<NumericType>())
            return Meta(BOOL, toInt(lhs) <= toInt(rhs));
        else if (dst == STRING)
            return Meta(BOOL, lhs.asString() <= rhs.asString());
        return Meta();
    }

    Meta greater(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(BOOL, toFloat(lhs) > toFloat(rhs));
        else if (dst->is<NumericType>())
            return Meta(BOOL, toInt(lhs) > toInt(rhs));
        else if (dst == STRING)
            return Meta(BOOL, lhs.asString() > rhs.asString());
        return Meta();
    }

    Meta greaterequal(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        const Type* dst = join(lhs.type(), rhs.type());
        if (!dst) return Meta();
        if (dst->is<NumericType>() && dst->as<NumericType>()->floating()) 
            return Meta(BOOL, toFloat(lhs) >= toFloat(rhs));
        else if (dst->is<NumericType>())
            return Meta(BOOL, toInt(lhs) >= toInt(rhs));
        else if (dst == STRING)
            return Meta(BOOL, lhs.asString() >= rhs.asString());
        return Meta();
    }

    Meta cons(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs.isList()) return Meta();
        if (!lhs.type()->explicitly(rhs.type()->as<ListType>()->element()))
            return Meta();
        return Meta();
    }

    Meta join(const Meta& lhs, const Meta& rhs) {
        if (!lhs || !rhs) return Meta();
        return Meta(find<TupleType>(vector<const Type*>({
            lhs.type(), rhs.type()
        })), new MetaTuple({ lhs, rhs }));
    }

    Meta unionf(const Meta& lhs, const Meta& rhs) {
        return Meta();
    }

    Meta intersect(const Meta& lhs, const Meta& rhs) {
        return Meta();
    }

    void assign(Meta& lhs, const Meta& rhs) {
        lhs = rhs;
    }
}

template<>
u64 hash(const basil::Meta& m) {
    return m.hash();
}

void print(stream& io, const basil::Meta& m) {
    m.format(io);
}

void print(const basil::Meta& m) {
    print(_stdout, m);
}