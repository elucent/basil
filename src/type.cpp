#include "type.h"
#include "io.h"

namespace basil {
    map<ustring, const Type*> typemap;

    // TypeClass

    TypeClass::TypeClass(): _parent(nullptr) {
        //
    }

    TypeClass::TypeClass(const TypeClass& parent): _parent(&parent) {
        //
    }

    const TypeClass* TypeClass::parent() const {
        return _parent;
    }

    u32 Type::nextid = 0;

    const TypeClass Type::CLASS;

    Type::Type(const ustring& key, u32 size, const TypeClass* tc):
        _typeclass(tc), _key(key), _size(size) {
        _id = nextid ++;
    }

    u32 Type::size() const {
        return _size;
    }

    u32 Type::id() const {
        return _id;
    }

    bool Type::implicitly(const Type* other) const {
        return other == this || other == ANY;
    }

    bool Type::explicitly(const Type* other) const {
        return other == this || other == ANY;
    }
    
    bool Type::conflictsWith(const Type* other) const {
        return this == other;
    }

    const ustring& Type::key() const {
        return _key;
    }

    void Type::format(stream& io) const {
        print(io, _key);
    }

    // NumericType

    const TypeClass NumericType::CLASS(Type::CLASS);

    NumericType::NumericType(u32 size, bool floating, bool sign, 
                             const TypeClass* tc):
        Type("", size, tc), _floating(floating), _signed(sign) {
        if (floating) _key += "f";
        else _key += sign ? "i" : "u";
        buffer b;
        fprint(b, size * 8);
        while (b) _key += b.read();
    }

    bool NumericType::floating() const {
        return _floating;
    }

    bool NumericType::isSigned() const {
        return _signed;
    }

    bool NumericType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        if (!other->is<NumericType>()) return false;
        const NumericType* nt = other->as<NumericType>();

        if (_floating) {
            return nt->_floating && nt->_size >= _size;
        }
        else {
            return !nt->_floating && nt->_signed == _signed;
        }
    }

    bool NumericType::explicitly(const Type* other) const {
        if (Type::explicitly(other)) return true;
        return other->is<NumericType>() &&
            (_floating ? other->as<NumericType>()->_floating : true);
    }

    // TupleType

    const TypeClass TupleType::CLASS(Type::CLASS);

    TupleType::TupleType(const vector<const Type*>& members,
                const TypeClass* tc):
        Type("", 0, tc), _members(members) {
        _key += "[T";
        for (const Type* t : members) {
            _offsets.push(_size);
            _size += t->size();
            _key += " ", _key += t->key();
        }
        _key += "]";
    }

    const vector<const Type*>& TupleType::members() const {
        return _members;
    }

    const Type* TupleType::member(u32 i) const {
        return _members[i];
    }

    u32 TupleType::offset(u32 i) const {
        return _offsets[i];
    }

    u32 TupleType::count() const {
        return _members.size();
    }

    bool TupleType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        if (!other->is<TupleType>()) return false;

        const TupleType* tt = other->as<TupleType>();
        if (tt->_members.size() != _members.size()) return false;

        for (u32 i = 0; i < _members.size(); ++ i) {
            if (!_members[i]->implicitly(tt->_members[i])) return false;
        }

        return true;
    }

    bool TupleType::explicitly(const Type* other) const {
        if (Type::explicitly(other)) return true;
        if (other == TYPE) {
            bool anyNonType = false;
            for (const Type* t : _members) if (t != TYPE) anyNonType = true;
            if (!anyNonType) return true;
        }
        if (!other->is<TupleType>()) return false;

        const TupleType* tt = other->as<TupleType>();
        if (tt->_members.size() != _members.size()) return false;

        for (u32 i = 0; i < _members.size(); ++ i) {
            if (!_members[i]->explicitly(tt->_members[i])) return false;
        }
        
        return true;
    }

    void TupleType::format(stream& io) const {
        print(io, "(");
        bool first = true;
        for (const Type* t : _members) {
            if (!first) print(io, ", ");
            first = false;
            t->format(io);
        }
        print(io, ")");
    }
        
    // BlockType

    const TypeClass BlockType::CLASS(Type::CLASS);

    BlockType::BlockType(const vector<const Type*>& members,
                const TypeClass* tc): 
        Type("", 0, tc), _members(members) {
        _key += "[T";
        for (const Type* t : members) {
            _key += " ", _key += t->key();
        }
        _key += "]";
    }
                
    const vector<const Type*>& BlockType::members() const {
        return _members;
    }
    
    const Type* BlockType::member(u32 i) const {
        return _members[i];
    }
    
    u32 BlockType::count() const {
        return _members.size();
    }
    
    bool BlockType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        if (!other->is<BlockType>()) return false;

        const BlockType* tt = other->as<BlockType>();
        if (tt->_members.size() != _members.size()) return false;

        for (u32 i = 0; i < _members.size(); ++ i) {
            if (!_members[i]->implicitly(tt->_members[i])) return false;
        }

        return true;
    }
    
    bool BlockType::explicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;

        if (other == TYPE) {
            bool anyNonType = false;
            for (const Type* t : _members) if (t != TYPE) anyNonType = true;
            if (!anyNonType) return true;
        }

        if (!other->is<BlockType>()) return false;

        const BlockType* tt = other->as<BlockType>();
        if (tt->_members.size() != _members.size()) return false;

        for (u32 i = 0; i < _members.size(); ++ i) {
            if (!_members[i]->explicitly(tt->_members[i])) return false;
        }

        return true;
    }
    
    void BlockType::format(stream& io) const {
        print(io, "[");
        bool first = true;
        for (const Type* t : _members) {
            if (!first) print(io, ", ");
            first = false;
            t->format(io);
        }
        print(io, "]");
    }

    // ArrayType

    const TypeClass ArrayType::CLASS(Type::CLASS);

    ArrayType::ArrayType(const Type* element, u32 size,
                const TypeClass* tc): 
        Type("", 0, tc), _element(element), _count(size) {
        _size = _element->size() * _count;
        buffer b;
        fprint(b, _element->key(), "[", _count, "]");
        fread(b, _key);
    }
                
    const Type* ArrayType::element() const {
        return _element;
    }
    
    u32 ArrayType::count() const {
        return _count;
    }
    
    bool ArrayType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        if (other->is<TupleType>()) {
            const TupleType* tt = other->as<TupleType>();
            for (const Type* t : tt->members()) {
                if (!_element->implicitly(t)) return false;
            }
            return _count == tt->count();
        }
        if (!other->is<ArrayType>()) return false;

        const ArrayType* at = other->as<ArrayType>();
        return _element->implicitly(at->element()) && _count == at->count();   
    }
    
    bool ArrayType::explicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        if (other->is<TupleType>()) {
            const TupleType* tt = other->as<TupleType>();
            for (const Type* t : tt->members()) {
                if (!_element->explicitly(t)) return false;
            }
            return _count == tt->count();
        }
        if (!other->is<ArrayType>()) return false;

        const ArrayType* at = other->as<ArrayType>();
        return _element->explicitly(at->element()) && _count == at->count();   
    }
    
    void ArrayType::format(stream& io) const {
        print(io, _element, "[", _count, "]");
    }

    // UnionType

    const TypeClass UnionType::CLASS(Type::CLASS);

    UnionType::UnionType(const set<const Type*>& members,
                         const TypeClass* tc):
        Type("", 0, tc), _members(members) {
        _key += "[U";
        for (const Type* t : members) {
            if (t->size() > _size) _size = t->size();
            _key += " ", _key += t->key();
        }
        _key += "]";
    }

    const set<const Type*>& UnionType::members() const {
        return _members;
    }

    bool UnionType::has(const Type* t) const {
        return _members.find(t) != _members.end();
    }

    bool UnionType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        return other == this;
    }

    bool UnionType::explicitly(const Type* other) const {
        if (Type::explicitly(other)) return true;
        if (other == TYPE) {
            bool anyNonType = false;
            for (const Type* t : _members) if (t != TYPE) anyNonType = true;
            if (!anyNonType) return true;
        }
        return has(other);
    }

    void UnionType::format(stream& io) const {
        print(io, "(");
        bool first = true;
        for (const Type* t : _members) {
            if (!first) print(io, " | ");
            first = false;
            t->format(io);
        }
        print(io, ")");
    }

    // IntersectionType

    const TypeClass IntersectionType::CLASS(Type::CLASS);

    IntersectionType::IntersectionType(const set<const Type*>& members,
                                       const TypeClass* tc):
        Type("", 0, tc), _members(members) {
        _key += "[I";
        bool overload = true;
        const Type* prev = nullptr;
        for (const Type* t : members) {
            if (prev && 
                (!prev->is<FunctionType>()
                 || !t->is<FunctionType>()
                 || t->as<FunctionType>()->conflictsWith(prev))) {
                overload = false;
            }
            prev = t;
            _size += t->size();
            _key += " ", _key += t->key();
        }
        if (overload) _size = prev->size();
        _key += "]";
    }

    const set<const Type*>& IntersectionType::members() const {
        return _members;
    }

    bool IntersectionType::has(const Type* t) const {
        return _members.find(t) != _members.end();
    }

    bool IntersectionType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        return this == other;
    }

    bool IntersectionType::explicitly(const Type* other) const {
        if (Type::explicitly(other)) return true;
        if (other == TYPE) {
            bool anyNonType = false;
            for (const Type* t : _members) if (t != TYPE) anyNonType = true;
            if (!anyNonType) return true;
        }
        return this == other;
    }

    bool IntersectionType::conflictsWith(const Type* other) const {
        for (const Type* t : _members) {
            if (t->conflictsWith(other)) return true;
        }
        return Type::conflictsWith(other);
    }

    void IntersectionType::format(stream& io) const {
        print(io, "(");
        bool first = true;
        for (const Type* t : _members) {
            if (!first) print(io, " & ");
            first = false;
            t->format(io);
        }
        print(io, ")");
    }
    
    // ListType
    
    const TypeClass ListType::CLASS(Type::CLASS);

    ListType::ListType(const Type* element, const TypeClass* tc):
        Type("", 8, tc), _element(element) {
        _key += "[L ";
        _key += element->key();
        _key += "]";
    }

    const Type* ListType::element() const {
        return _element;
    }

    bool ListType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        return other == this;
    }

    bool ListType::explicitly(const Type* other) const {
        return implicitly(other); // todo: consider reification?
    }

    void ListType::format(stream& io) const {
        print(io, "[", _element, "]");
    }
    
    // ReferenceType
    
    const TypeClass ReferenceType::CLASS(Type::CLASS);

    ReferenceType::ReferenceType(const Type* element, const TypeClass* tc):
        Type("", 8, tc), _element(element) {
        _key += "[R ";
        _key += element->key();
        _key += "]";
    }

    const Type* ReferenceType::element() const {
        return _element;
    }

    bool ReferenceType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        return other == _element || other == this;
    }

    bool ReferenceType::explicitly(const Type* other) const {
        if (other == TYPE) return _element == TYPE;
        return implicitly(other); // todo: consider reification?
    }

    void ReferenceType::format(stream& io) const {
        print(io, "~", _element);
    }

    // EmptyType

    const TypeClass EmptyType::CLASS(Type::CLASS);

    EmptyType::EmptyType(const TypeClass* tc):
        Type("[empty]", 8, tc) {
        //
    }

    bool EmptyType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        return other == this || other->is<ListType>();
    }

    bool EmptyType::explicitly(const Type* other) const {
        return implicitly(other);
    }

    void EmptyType::format(stream& io) const {
        print(io, "[]");
    }
    
    // MacroType

    const TypeClass MacroType::CLASS(Type::CLASS);

    MacroType::MacroType(const Type* arg, bool quoting, 
                         const vector<Constraint>& cons,
                         const TypeClass* tc):
        Type("", 0, tc), _arg(arg), _cons(cons), _quoting(quoting) {
        _key += (quoting ? "[QM " : "[M ");
        _key += arg->key();
        _key += " { ";
        for (auto& con : cons) _key += con.key() + " ";
        _key += "} ";
        _key += "]";
    }

    MacroType::MacroType(const Type* arg,
                         const vector<Constraint>& cons,
                         const TypeClass* tc):
        MacroType(arg, false, cons, tc) {
        //
    }

    const Type* MacroType::arg() const {
        return _arg;
    }

    bool MacroType::quoting() const {
        return _quoting;
    }
        
    const vector<Constraint>& MacroType::constraints() const {
        return _cons;
    }

    bool MacroType::conflictsWith(const Type* other) const {
        if (!other->is<MacroType>()) return true;
        const MacroType* ft = other->as<MacroType>();
        if (ft->_arg != _arg) return false; 
        for (auto& a : _cons) for (auto& b : ft->_cons) {
            if (a.conflictsWith(b)) return true;
        }
        return false;
    }

    bool MacroType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        if (!other->is<MacroType>()) return false;

        const MacroType* ft = other->as<MacroType>();

        return ft->_arg == _arg && ft->_quoting == _quoting;
    }

    bool MacroType::explicitly(const Type* other) const {
        return implicitly(other);
    }

    void MacroType::format(stream& io) const {
        print(io, "(", _arg, _quoting ? " quoting-macro)" : " macro)");
    }

    Constraint MacroType::matches(Meta fr) const {
        return maxMatch(_cons, fr);
    }

    const TypeClass FunctionType::CLASS(Type::CLASS);

    FunctionType::FunctionType(const Type* arg, const Type* ret,
                               const vector<Constraint>& cons,
                               const TypeClass* tc):
        FunctionType(arg, ret, false, cons, tc) {
        //
    }

    FunctionType::FunctionType(const Type* arg, const Type* ret, 
                               bool quoting, const vector<Constraint>& cons,
                               const TypeClass* tc):
        Type("", 0, tc), _arg(arg), _ret(ret), 
        _cons(cons), _quoting(quoting) {
        if (!_cons.size()) _cons.push(Constraint());
        _key += (quoting ? "[QF " : "[F ");
        _key += arg->key();
        _key += " ";
        _key += ret->key();
        _key += " { ";
        for (auto& con : cons) _key += con.key() + " ";
        _key += "} ";
        _key += "]";
        _size = 8;
    }

    const Type* FunctionType::FunctionType::arg() const {
        return _arg;
    }

    const Type* FunctionType::ret() const {
        return _ret;
    }

    bool FunctionType::quoting() const {
        return _quoting;
    }

    const vector<Constraint>& FunctionType::constraints() const {
        return _cons;
    }

    bool FunctionType::total() const {
        vector<Meta> vals;
        for (const Constraint& c : _cons) {
            if (c.type() == OF_TYPE || c.type() == UNKNOWN) return true;
            else vals.push(c.value());
        }
        // todo: exhaustive cases
        return false;
    }

    bool FunctionType::conflictsWith(const Type* other) const {
        if (other->is<MacroType>()) return true;
        if (!other->is<FunctionType>()) return false;
        const FunctionType* ft = other->as<FunctionType>();
        if (ft->_arg != _arg && ft->_ret != _ret) return false; 
        for (auto& a : _cons) for (auto& b : ft->_cons) {
            if (a.conflictsWith(b)) return true;
        }
        return false;
    }

    bool FunctionType::implicitly(const Type* other) const {
        if (Type::implicitly(other)) return true;
        if (!other->is<FunctionType>()) return false;

        const FunctionType* ft = other->as<FunctionType>();

        if (ft->_quoting != _quoting) return false;

        if (ft->_ret != _ret || ft->_arg != _arg) return false;
        return ft->constraints().size() == 1 
            && ft->constraints()[0].type() == UNKNOWN;
    }

    bool FunctionType::explicitly(const Type* other) const {
        if (Type::explicitly(other)) return true;
        if (other == TYPE) {
            return _arg == TYPE && _ret == TYPE && _cons.size() == 1
                && _cons[0].type() == EQUALS_VALUE 
                && _cons[0].value().isType();
        }
        return implicitly(other);
    }

    void FunctionType::format(stream& io) const {
        print(io, "(");
        if (_cons.size() == 1 && _cons[0].type() == EQUALS_VALUE) {
            print(io, _cons[0].value().toString());
        }
        else _arg->format(io);
        print(io, _quoting ? " => " : " -> ");
        _ret->format(io);
        print(io, ")");
    }

    Constraint FunctionType::matches(Meta fr) const {
        return maxMatch(_cons, fr);
    }

    Constraint maxMatch(const vector<Constraint>& cons, Meta fr) {
        if (cons.size() == 0) return Constraint();
        Constraint ret = Constraint::NONE;
        for (const Constraint& c : cons) if (c.matches(fr)) {
            if (c.precedes(ret) || ret.type() == UNKNOWN
                || ret.type() == NULL_CONSTRAINT) ret = c;
        }
        return ret;
    }

    // Constraint
    
    Constraint::Constraint(ConstraintType type):
        _type(type) {
        _key = "";
    }

    Constraint::Constraint(Meta value):
        _type(EQUALS_VALUE), _value(value) {
        _key = "(= ";
        _key += value.toString();
        _key += ")";
    }

    Constraint::Constraint(const Type* type):
        _type(OF_TYPE), _value(Meta(TYPE, type)) {
        _key = "(: ";
        _key += type->key();
        _key += ")";
    }

    Constraint::Constraint():
        _type(UNKNOWN) {
        _key = "(?)";
    }

    const Constraint Constraint::NONE(NULL_CONSTRAINT);
    
    ConstraintType Constraint::type() const {
        return _type;
    }

    Meta Constraint::value() const {
        return _value;
    }

    Constraint::operator bool() const {
        return _type != NULL_CONSTRAINT;
    }

    bool Constraint::conflictsWith(const Constraint& other) const {
        if (_type == UNKNOWN) return true;
        if (_type == other._type && _type == EQUALS_VALUE) {
            return _value == other._value;
        }
        if (_type == OF_TYPE) return other._type == OF_TYPE;
        return false;
    }

    bool Constraint::precedes(const Constraint& other) const {
        return _type < other._type;
    }

    const ustring& Constraint::key() const {
        return _key;
    }

    bool Constraint::matches(Meta value) const {
        if (_type == UNKNOWN || _type == OF_TYPE) return true;
        if (_type == EQUALS_VALUE) return value == _value;
        return false;
    }
    
    const Type* join(const Type* a, const Type* b) {
        if (a == b) return a;
        else if (a->implicitly(b)) return b;
        else if (b->implicitly(a)) return a;
        else if (a->explicitly(b)) return b;
        else if (b->explicitly(a)) return a;
        else return nullptr;
    }

    const Type *I8 = find<NumericType>(1, false, true), 
               *I16 = find<NumericType>(2, false, true), 
               *I32 = find<NumericType>(4, false, true), 
               *I64 = find<NumericType>(8, false, true), 
               *U8 = find<NumericType>(1, false, false), 
               *U16 = find<NumericType>(2, false, false), 
               *U32 = find<NumericType>(4, false, false), 
               *U64 = find<NumericType>(8, false, false), 
               *FLOAT = find<NumericType>(4, true, true), 
               *DOUBLE = find<NumericType>(8, true, true),
               *BOOL = find<Type>("bool", 1), 
               *TYPE = find<Type>("type", 4), 
               *SYMBOL = find<Type>("symbol", 4),
               *ERROR = find<Type>("error", 1),
               *EMPTY = find<EmptyType>(),
               *VOID = find<Type>("void", 1),
               *ANY = find<Type>("any", 1),
               *UNDEFINED = find<Type>("undefined", 1),
               *STRING = find<Type>("string", 8),
               *CHAR = find<Type>("char", 4);
    
    bool isGC(const Type* t) {
        return t == STRING 
            || t->is<ListType>();
    }
}

void print(stream& io, const basil::Type* t) {
    t->format(io);
}

void print(const basil::Type* t) {
    print(_stdout, t);
}