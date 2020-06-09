#include "value.h"
#include "io.h"
#include "type.h"
#include "term.h"
#include "lex.h"
#include "parse.h"
#include "source.h"
#include "errors.h"
#include "import.h"
#include "ir.h"

namespace basil {

    // Stack

    Stack::Entry::Entry(const Type* type, Stack::builtin_t builtin):
        _type(type), _meta(nullptr), _builtin(builtin),
        _loc(nullptr), _reassigned(false), _storage(STORAGE_LOCAL) {
        //
    }

    Stack::Entry::Entry(const Type* type, const Meta& value, builtin_t builtin):
        _type(type), _value(value), _meta(nullptr), 
        _builtin(builtin),
        _loc(nullptr), _reassigned(false), _storage(STORAGE_LOCAL) {
        //
    }

    Stack::Entry::Entry(const Type* type, Value* meta, builtin_t builtin):
        _type(type), _meta(meta), _builtin(builtin),
        _loc(nullptr), _reassigned(false), _storage(STORAGE_LOCAL) {
        //
    }
    
    const Type* Stack::Entry::type() const {
        return _type;
    }

    Stack::builtin_t Stack::Entry::builtin() const {
        return _builtin;
    }

    const Meta& Stack::Entry::value() const {
        return _value;
    }

    Meta& Stack::Entry::value() {
        return _value;
    }

    Value* Stack::Entry::meta() const {
        return _meta;
    }

    Location* Stack::Entry::loc() const {
        return _loc;
    }
    
    const Type*& Stack::Entry::type() {
        return _type;
    }

    Stack::builtin_t& Stack::Entry::builtin() {
        return _builtin;
    }

    Value*& Stack::Entry::meta()  {
        return _meta;
    }

    Location*& Stack::Entry::loc() {
        return _loc;
    }

    Storage Stack::Entry::storage() const {
        return _storage;
    }

    Storage& Stack::Entry::storage() {
        return _storage;
    }

    bool Stack::Entry::reassigned() const {
        return _reassigned;
    }

    void Stack::Entry::reassign() {
        _reassigned = true;
    }

    ustring interactName(const Type* a, const Type* b) {
        return "#[" + a->key() + " " + b->key() + "]";
    }

    const Type* Stack::tryApply(const Type* func, Value* arg,
                                u32 line, u32 column) {
        const Type* t = func;
        const Type* argt = arg->type(*this);
        if (t->is<FunctionType>()) {
            const FunctionType* ft = t->as<FunctionType>();
            if (argt->explicitly(ft->arg())) {
                return ft;
            }
            else return nullptr;
        }
        if (t->is<IntersectionType>()) {
            const IntersectionType* it = t->as<IntersectionType>();
            vector<const Type*> fns;
            for (const Type* t : it->members()) {
                if (auto ft = tryApply(t, arg, line, column)) {
                    fns.push(ft);
                }
            }
            if (fns.size() > 1) {
                bool equalFound = false;
                bool implicitFound = false;
                bool nonAnyFound = false;
                for (const Type* ft : fns) {
                    const Type* arg = ft->as<FunctionType>()->arg();
                    if (argt == arg) equalFound = true;
                    if (argt->implicitly(arg)) implicitFound = true;
                    if (arg != ANY) nonAnyFound = true;
                }

                if (equalFound) {
                    const Type** first = fns.begin();
                    const Type** iter = first;
                    u32 newsize = 0;
                    while (iter != fns.end()) {
                        const Type* arg = (*iter)->as<FunctionType>()->arg();
                        if (argt == arg) {
                            *(first ++) = *iter, ++ newsize;
                        }
                        ++ iter;
                    }
                    while (fns.size() > newsize) fns.pop();
                }
                else if (implicitFound) {
                    const Type** first = fns.begin();
                    const Type** iter = first;
                    u32 newsize = 0;
                    while (iter != fns.end()) {
                        const Type* arg = (*iter)->as<FunctionType>()->arg();
                        if (argt->implicitly(arg)) {
                            *(first ++) = *iter, ++ newsize;
                        }
                        ++ iter;
                    }
                    while (fns.size() > newsize) fns.pop();
                }
                else if (nonAnyFound) {
                    const Type** first = fns.begin();
                    const Type** iter = first;
                    u32 newsize = 0;
                    while (iter != fns.end()) {
                        const Type* arg = (*iter)->as<FunctionType>()->arg();
                        if (arg != ANY) {
                            *(first ++) = *iter, ++ newsize;
                        }
                        ++ iter;
                    }
                    while (fns.size() > newsize) fns.pop();
                }
            }

            if (fns.size() > 1) {
                buffer b;
                fprint(b, "Ambiguous application of overloaded function ",
                       "or macro for argument type '", argt, 
                       "'. Candidates were:");
                for (const Type* fn : fns) {
                    fprint(b, '\n');
                    fprint(b, "    ", (const Type*)fn);
                }
                err(PHASE_TYPE, line, column, b);
            }
            else if (fns.size() == 1) {
                return fns[0];
            }
            else return nullptr;
        }

        return nullptr;
    }

    const Type* Stack::tryApply(Value* func, Value* arg) {
        return tryApply(func->type(*this), arg, func->line(), func->column());
    }

    Stack::Entry* Stack::tryInteract(Value* first, Value* second) {
        Stack* s = this;
        while (s && !s->tmcache) {
            s = s->_parent;
        }
        if (!s) return nullptr;
        tmcache_t* cache = s->tmcache;

        const Type* ft = first->type(*this);
        const Type* st = second->type(*this);
        auto it = cache->find(ft);
        if (it == cache->end()) {
            set<pair<const Type*, const Type*>> methods;
            Stack* s = this;
            while (s && !s->tmcache) s = s->_parent;
            while (s && s->tmcache) {
                for (auto& e : *s->tmethods) {
                    if (ft->explicitly(e.first.first)) {
                        methods.insert({ e.first.first, e.first.second });
                    }
                }

                // traverse up
                s = s->_parent;
                while (s && !s->tmcache) s = s->_parent;
            }
            cache->put(ft, methods);

            it = cache->find(ft);
        }

        vector<Entry*> entries;
        for (auto& p : it->second) {
            if (st->explicitly(p.second)) entries.push(interactOf(p.first, p.second));
        }
        if (entries.size() == 0) return nullptr;
        else return entries[0];
    }

    Value* Stack::apply(Value* func, const Type* ft, Value* arg) {
        auto e = func->entry(*this);
        if (e && e->builtin()) func = e->builtin()(func);
        if (func->is<Builtin>() && func->as<Builtin>()->canApply(*this, arg)) {
            return func->as<Builtin>()->apply(*this, arg);
        }
        else {
            Meta m = func->fold(*this);
            if (m.isFunction() && m.asFunction().value()->is<Builtin>()
                && m.asFunction().value()->as<Builtin>()->canApply(*this, arg)) {
                return m.asFunction().value()->as<Builtin>()->apply(*this, arg);
            }
            return new Call(func, ft, arg, func->line(), func->column());
        }
    }
    
    Stack::Stack(Stack* parent, bool scope): 
        _parent(parent),
        table(scope ? new map<ustring, Entry>() : nullptr),
        tmethods(scope ? new tmscope_t() : nullptr),
        tmcache(scope ? new tmcache_t() : nullptr),
        _depth(parent ? parent->depth() + 1 : 0) {
        if (parent) parent->_children.push(this);
        if (tmcache) {
            // copy nearest parent's tmcache
            Stack* s = _parent;
            while (s && !s->table && s->_parent) s = s->_parent;
            if (s) *tmcache = *s->tmcache;
        }
    }

    Stack::~Stack() {
        if (table) delete table;
        for (Stack* s : _children) delete s;
    }

    Stack::Stack(const Stack& other): 
        _parent(other._parent), values(other.values), _depth(other.depth()) {
        table = other.table ? new map<ustring, Entry>(*other.table) : nullptr;
        tmethods = other.tmethods ? new tmscope_t(*other.tmethods) : nullptr;
        tmcache = other.tmcache ? new tmcache_t(*other.tmcache) : nullptr;
        for (Stack* s : other._children) {
            _children.push(new Stack(*s));
        }
    }

    Stack& Stack::operator=(const Stack& other) {
        if (this != &other) {
            if (table) delete table;
            if (tmethods) delete tmethods;
            if (tmcache) delete tmcache;
            for (Stack* s : _children) delete s;
            _parent = other._parent;
            values = other.values;
            table = other.table ? new map<ustring, Entry>(*other.table) : nullptr;
            tmethods = other.tmethods ? new tmscope_t(*other.tmethods) : nullptr;
            tmcache = other.tmcache ? new tmcache_t(*other.tmcache) : nullptr;
            for (Stack* s : other._children) {
                _children.push(new Stack(*s));
            }
        }
        return *this;
    }

    void Stack::detachTo(Stack& s) {
        table = nullptr;
        for (Stack* c : _children) {
            c->_parent = &s;
            s._children.push(c);
        }
        _children.clear();
    }

    const Value* Stack::top() const {
        return values.back();
    }

    Value* Stack::top() {
        return values.back();
    }

    const Value* const* Stack::begin() const {
        return values.begin();
    }

    Value** Stack::begin() {
        return values.begin();
    }

    const Value* const* Stack::end() const {
        return values.end();
    }

    Value** Stack::end() {
        return values.end();
    }
    
    bool Stack::expectsMeta() {
        if (size() == 0) return false;

        const Type* tt = top()->type(*this);
        if (tt->is<FunctionType>() && tt->as<FunctionType>()->quoting()) 
            return true;
        else if (tt->is<IntersectionType>()) {
            for (const Type* t : tt->as<IntersectionType>()->members()) {
                if (t->is<FunctionType>() && t->as<FunctionType>()->quoting()) 
                    return true;
            }
        }
        return false;
    }

    void Stack::push(Value* v) {
        if (!v) return;
        // try declaration
        if (values.size() && top()->type(*this)->explicitly(TYPE) 
            && v->is<Variable>() 
            && (!v->entry(*this) || !tryApply(v, top()))) {
            if (top()->type(*this) != TYPE) {
                values.push(new Cast(TYPE, pop()));
            }
            Define* d = new Define(pop(), v->as<Variable>()->name());
            d->apply(*this, nullptr);
            return push(d);
        }

        // try interaction
        if (!values.size()) {
            values.push(v);
            return;
        }
        else if (Entry* e = tryInteract(top(), v)) {
            Value* first = pop(), *second = v;
            values.push(second);
            values.push(first);
            if (e->builtin()) push(e->builtin()(first));
            else push(new Interaction(e->value(), first->line(), second->line()));
            return;
        }
        else if (Entry* e = tryInteract(v, top())) {
            values.push(v);
            if (e->builtin()) push(e->builtin()(v));
            else push(new Interaction(e->value(), v->line(), v->line()));   
            return; 
        }

        // try function application
        if (!values.size()) {
            values.push(v);
            return;
        }
        else if (const Type* ft = tryApply(top(), v)) {
            const Type* arg = ft->as<FunctionType>()->arg();
            if (v->type(*this) != arg && !arg->wildcard()) {
                v = new Cast(arg, v);
            }
            Value* result = apply(pop(), ft, v);
            if (result) push(result);
        }
        else if (const Type* ft = tryApply(v, top())) {
            const Type* arg = ft->as<FunctionType>()->arg();
            if (top()->type(*this) != arg && !arg->wildcard()) {
                values.back() = new Cast(arg, top());
            }
            Value* result = apply(v, ft, pop());
            if (result) push(result);
        }
        else values.push(v);
    }

    Value* Stack::pop() {
        Value* v = values.back();
        values.pop();
        return v;
    }

    bool Stack::hasScope() const {
        return table;
    }

    const Stack::Entry* Stack::operator[](const ustring& name) const {
        if (table) {
            auto it = table->find(name);
            if (it != table->end()) return &(it->second);
        }
        if (_parent) return (*_parent)[name];
        return nullptr;
    }

    Stack::Entry* Stack::operator[](const ustring& name) {
        if (table) {
            auto it = table->find(name);
            if (it != table->end()) return &(it->second);
        }
        if (_parent) return (*_parent)[name];
        return nullptr;
    }

    const Stack* Stack::findenv(const ustring& name) const {
        if (table) {
            auto it = table->find(name);
            if (it != table->end()) return this;
        }
        if (_parent) return _parent->findenv(name);
        return nullptr;
    }

    Stack* Stack::findenv(const ustring& name) {
        if (table) {
            auto it = table->find(name);
            if (it != table->end()) return this;
        }
        if (_parent) return _parent->findenv(name);
        return nullptr;
    }

    Stack::Entry* Stack::interactOf(const Type* a, const Type* b) {
        if (tmethods) {
            auto it = tmethods->find({ a, b });
            if (it != tmethods->end()) {
                return &(it->second);
            }
        }
        if (_parent) return _parent->interactOf(a, b);
        return nullptr;
    }

    void Stack::interact(const Type* a, const Type* b, const Meta& f) {
        const FunctionType* ft = find<FunctionType>(a, find<FunctionType>(b, ANY));
        if (tmethods) {
            (*tmethods)[{a, b}] = Entry(ft, f);
            for (auto& p : *tmcache) if (p.first->explicitly(a)) 
                p.second.insert({ a, b });
        }
        else if (_parent) _parent->interact(a, b, f);
    }

    void Stack::interact(const Type* a, const Type* b, builtin_t f) {
        const FunctionType* ft = find<FunctionType>(a, find<FunctionType>(b, ANY));
        if (tmethods) {
            (*tmethods)[{a, b}] = Entry(ft, f);
            for (auto& p : *tmcache) if (p.first->explicitly(a))  
                p.second.insert({ a, b });
        }
        else if (_parent) _parent->interact(a, b, f);
    }

    void Stack::bind(const ustring& name, const Type* t) {
        if (table) (*table)[name] = Entry(t);
        else if (_parent) _parent->bind(name, t);
    }

    void Stack::bind(const ustring& name, const Type* t, const Meta& f) {
        if (table) (*table)[name] = Entry(t, f);
        else if (_parent) _parent->bind(name, t, f);
    }

    void Stack::bind(const ustring& name, const Type* t, builtin_t b) {
        if (table) (*table)[name] = Entry(t, b);
        else if (_parent) _parent->bind(name, t, b);
    }

    void Stack::bind(const ustring& name, const Type* t, Value* v) {
        if (table) (*table)[name] = Entry(t, v);
        else if (_parent) _parent->bind(name, t, v);
    }

    void Stack::erase(const ustring& name) {
        if (table) table->erase(name);
    }

    void Stack::clear() {
        values.clear();
    }

    void Stack::copy(Stack& other) {
        for (Value* v : other) values.push(v);
    }

    void Stack::copy(vector<Value*>& other) {
        for (Value* v : other) values.push(v);
    }

    u32 Stack::size() const {
        return values.size();
    }
    
    const map<ustring, Stack::Entry>& Stack::nearestScope() const {
        const Stack* s = this;
        const map<ustring, Stack::Entry>* t = table;
        while (s->_parent && !t) {
            s = s->_parent;
            t = s->table;
        }
        return *t;
    }
    
    map<ustring, Stack::Entry>& Stack::nearestScope() {
        Stack* s = this;
        map<ustring, Stack::Entry>* t = table;
        while (s->_parent && !t) {
            s = s->_parent;
            t = s->table;
        }
        return *t;
    }
    
    const map<ustring, Stack::Entry>& Stack::scope() const {
        return *table;
    }
    
    map<ustring, Stack::Entry>& Stack::scope() {
        return *table;
    }
    
    Stack* Stack::parent() {
        return _parent;
    }

    const Stack* Stack::parent() const {
        return _parent;
    }

    u32 Stack::depth() const {
        return _depth;
    }

    ustring& Stack::name() {
        return _name;
    }

    const ustring& Stack::name() const {
        return _name;
    }

    void stacktrace(Stack* s) {
        while (s) {
            println(_stdout, "Stack ", s->name(), ":");
            if (s->hasScope()) {
                println(_stdout, "{");
                for (auto& p : s->scope()) {
                    println(_stdout, "    ", p.first, ": ", p.second.type(), " = ", p.second.value());
                }
                println(_stdout, "}");
            }
            println(_stdout, "");
            println(_stdout, " |");
            println(_stdout, " v");
            println(_stdout, "");
            s = s->parent();
        }
    }

    // ValueClass

    ValueClass::ValueClass(): _parent(nullptr) {
        //
    }

    ValueClass::ValueClass(const ValueClass& parent): _parent(&parent) {
        //
    }

    const ValueClass* ValueClass::parent() const {
        return _parent;
    }

    // Value

    const ValueClass Value::CLASS;

    void Value::indent(stream& io, u32 level) const {
        while (level) -- level, print(io, "    ");
    }

    void Value::setType(const Type* t) {
        _cachetype = t;
    }

    const Type* Value::lazyType(Stack& ctx) {
        return ERROR;
    }

    void Value::updateType(Stack& ctx) {
        setType(lazyType(ctx));
    }

    Value::Value(u32 line, u32 column, const ValueClass* vc):
        _line(line), _column(column), _valueclass(vc), _cachetype(nullptr) {
        //
    }

    Value::~Value() {
        //
    }

    u32 Value::line() const {
        return _line;
    }

    u32 Value::column() const {
        return _column;
    }

    Meta Value::fold(Stack& ctx) {
        return Meta();
    }
    
    bool Value::lvalue(Stack& ctx) {
        return false;
    }
    
    Stack::Entry* Value::entry(Stack& ctx) const {
        return nullptr;
    }

    const Type* Value::type(Stack& ctx) {
        if (!_cachetype) setType(lazyType(ctx));
        return _cachetype;
    }

    Location* Value::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.none();
    }
    
    void Value::explore(Explorer& e) {
        e.visit(this);
    }

    // Builtin

    const ValueClass Builtin::CLASS(Value::CLASS);

    Builtin::Builtin(u32 line, u32 column, const ValueClass* vc):
        Value(line, column, vc) {
        //
    }

    bool Builtin::canApply(Stack& ctx, Value* arg) const {
        return true;
    }

    // Void
    
    const ValueClass Void::CLASS(Value::CLASS);

    Void::Void(u32 line, u32 column, const ValueClass* vc):
        Value(line, column, vc) {
        setType(VOID);
    }

    void Void::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Void ()");
    }

    Meta Void::fold(Stack& ctx) {
        return Meta(VOID);
    }

    Location* Void::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.none();
    }

    Value* Void::clone(Stack& ctx) const {
        return new Void(line(), column());
    }

    void Void::repr(stream& io) const {
        print(io, "()");
    }

    // Empty

    const ValueClass Empty::CLASS(Value::CLASS);

    Empty::Empty(u32 line, u32 column, const ValueClass* vc):
        Value(line, column, vc) {
        setType(EMPTY);
    }

    void Empty::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Empty []");
    }

    Meta Empty::fold(Stack& ctx) {
        // todo: empty list constexpr
        return Meta(VOID);
    }

    Location* Empty::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        // todo: empty list codegen
        return frame.none();
    }

    Value* Empty::clone(Stack& ctx) const {
        return new Empty(line(), column());
    }

    void Empty::repr(stream& io) const {
        print(io, "[]");
    }

    // IntegerConstant

    const ValueClass IntegerConstant::CLASS(Value::CLASS);

    IntegerConstant::IntegerConstant(i64 value, u32 line, u32 column, 
                                     const ValueClass* tc):
        Value(line, column, tc), _value(value) {
        setType(I64);
    }

    i64 IntegerConstant::value() const {
        return _value;
    }

    void IntegerConstant::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Integer ", _value);
    }

    Meta IntegerConstant::fold(Stack& ctx) {
        return Meta(type(ctx), _value);
    }
    
    Location* IntegerConstant::gen(Stack& ctx, 
        CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(new IntData(_value))->value(gen, frame);
    }

    Value* IntegerConstant::clone(Stack& ctx) const {
        return new IntegerConstant(_value, line(), column());
    }

    void IntegerConstant::repr(stream& io) const {
        print(io, _value);
    }

    // RationalConstant

    const ValueClass RationalConstant::CLASS(Value::CLASS);

    RationalConstant::RationalConstant(double value, u32 line, u32 column, 
                                       const ValueClass* tc):
        Value(line, column, tc), _value(value) {
        setType(DOUBLE);
    }

    double RationalConstant::value() const {
        return _value;
    }

    void RationalConstant::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Float ", _value);
    }

    Meta RationalConstant::fold(Stack& ctx) {
        return Meta(type(ctx), _value);
    }
    
    Location* RationalConstant::gen(Stack& ctx, 
        CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(new FloatData(_value))->value(gen, frame);
    }

    Value* RationalConstant::clone(Stack& ctx) const {
        return new RationalConstant(_value, line(), column());
    }

    void RationalConstant::repr(stream& io) const {
        print(io, _value);
    }

    // StringConstant

    const ValueClass StringConstant::CLASS(Value::CLASS);

    StringConstant::StringConstant(const ustring& value, u32 line, u32 column, 
                    const ValueClass* vc):
        Value(line, column, vc), _value(value) {
        setType(STRING);
    }

    const ustring& StringConstant::value() const {
        return _value;
    }

    void StringConstant::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "String \"", escape(_value), "\"");
    }

    Meta StringConstant::fold(Stack& ctx) {
        return Meta(type(ctx), _value);
    }

    Location* StringConstant::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(new StrData(_value))->value(gen, frame);
    }

    Value* StringConstant::clone(Stack& ctx) const {
        return new StringConstant(_value, line(), column());
    }

    void StringConstant::repr(stream& io) const {
        print(io, '"', _value, '"');
    }

    // CharConstant

    const ValueClass CharConstant::CLASS(Value::CLASS);

    CharConstant::CharConstant(uchar value, u32 line, u32 column, 
                    const ValueClass* vc):
        Value(line, column, vc), _value(value) {
        setType(CHAR);
    }

    uchar CharConstant::value() const {
        return _value;
    }

    void CharConstant::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Character '", _value, "'");
    }

    Meta CharConstant::fold(Stack& ctx) {
        return Meta();
        // TODO char foldresult
    }

    Value* CharConstant::clone(Stack& ctx) const {
        return new CharConstant(_value, line(), column());
    }

    void CharConstant::repr(stream& io) const {
        print(io, '\'', _value, '\'');
    }

    // TypeConstant

    const ValueClass TypeConstant::CLASS(Value::CLASS);

    TypeConstant::TypeConstant(const Type* value, u32 line, u32 column,
                               const ValueClass* vc):
        Value(line, column, vc), _value(value) {
        setType(TYPE);
    }

    const Type* TypeConstant::value() const {
        return _value;
    }

    void TypeConstant::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Type ", _value);
    }

    Meta TypeConstant::fold(Stack& ctx) {
        return Meta(type(ctx), _value);
    }

    Value* TypeConstant::clone(Stack& ctx) const {
        return new TypeConstant(_value, line(), column());
    }

    void TypeConstant::repr(stream& io) const {
        print(io, _value);
    }

    // BoolConstant

    const ValueClass BoolConstant::CLASS(Value::CLASS);

    BoolConstant::BoolConstant(bool value, u32 line, u32 column,
                    const ValueClass* vc):
        Value(line, column, vc), _value(value) {
        setType(BOOL);
    }

    bool BoolConstant::value() const {
        return _value;
    }

    void BoolConstant::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Boolean ", _value);
    }

    Meta BoolConstant::fold(Stack& ctx) {
        return Meta(type(ctx), _value);
    }
    
    Location* BoolConstant::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(new BoolData(_value))->value(gen, frame);
    }

    Value* BoolConstant::clone(Stack& ctx) const {
        return new BoolConstant(_value, line(), column());
    }

    void BoolConstant::repr(stream& io) const {
        print(io, _value);
    }

    // Quote

    const ValueClass Quote::CLASS(Builtin::CLASS);

    Quote::Quote(u32 line, u32 column, const ValueClass* vc):
        Builtin(line, column, vc), _term(nullptr) {
        setType(find<FunctionType>(ANY, ANY));
    }

    Quote::Quote(Term* term, u32 line, u32 column,
                 const ValueClass* vc):
        Builtin(line, column, vc), _term(term) {
        setType(term->type());
    }

    Term* Quote::term() const {
        return _term;
    }

    void Quote::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Quote");
        _term->format(io, level + 1);
    }

    Meta Quote::fold(Stack& ctx) {
        if (!_term) return Meta();
        return _term->fold();
    }

    Value* Quote::clone(Stack& ctx) const {
        if (!_term) return new Quote(line(), column());
        return new Quote(_term, line(), column());
    }

    bool Quote::canApply(Stack& ctx, Value* v) const {
        return !_term;
    }

    Value* Quote::apply(Stack& ctx, Value* v) {
        delete this;
        return v;
    }

    void Quote::repr(stream& io) const {
        print(io, "(quote ", _term, ")");
    }

    // Incomplete

    const ValueClass Incomplete::CLASS(Value::CLASS);

    Incomplete::Incomplete(Term* term, u32 line, u32 column,
                const ValueClass* vc):
        Value(line, column, vc), _term(term) {
        setType(ANY);
    }
    
    Term* Incomplete::term() const {
        return _term;
    }

    void Incomplete::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Incomplete");
        _term->format(io, level + 1);
    }

    Value* Incomplete::clone(Stack& ctx) const {
        return new Incomplete(_term, line(), column());
    }

    void Incomplete::repr(stream& io) const {
        print(io, "???");
    }

    // Variable

    const ValueClass Variable::CLASS(Value::CLASS);

    const Type* Variable::lazyType(Stack& ctx) {
        const Stack::Entry* entry = ctx[_name];
        if (!entry) {
            err(PHASE_TYPE, line(), column(),
                "Undeclared variable '", _name, "'.");
            return ERROR;
        }
        return entry->type();
    }

    Variable::Variable(const ustring& name, u32 line, u32 column,
                       const ValueClass* vc):
        Value(line, column, vc), _name(name) {
        //
    }

    const ustring& Variable::name() const {
        return _name;
    }

    void Variable::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Variable ", _name);
    }

    Meta Variable::fold(Stack& ctx) {
        const Stack::Entry* entry = ctx[_name];
        if (!entry) {
            err(PHASE_TYPE, line(), column(),
                "Undeclared variable '", _name, "'.");
            return {};
        }
        return entry->value();
    }

    bool Variable::lvalue(Stack& ctx) {
        return true;
    }

    Stack::Entry* Variable::entry(Stack& ctx) const {
        return ctx[_name];
    }
    
    Location* Variable::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return entry(ctx)->loc();
    }

    Value* Variable::clone(Stack& ctx) const {
        return new Variable(_name, line(), column());
    }

    void Variable::repr(stream& io) const {
        print(io, _name);
    }

    // Interaction

    const ValueClass Interaction::CLASS(Value::CLASS);

    Interaction::Interaction(const Meta& fn, u32 line, u32 column, 
                const ValueClass* vc):
        Value(line, column, vc), _fn(fn) {
        setType(_fn.type());
    }

    void Interaction::format(stream& io, u32 level) const {
        indent(io, level);
        const FunctionType* ft = _fn.type()->as<FunctionType>();
        println(io, "Interaction ", ft->arg(), " ", ft->ret());
    }

    Meta Interaction::fold(Stack& ctx) {
        return _fn;
    }

    Value* Interaction::clone(Stack& ctx) const {
        return new Interaction(_fn, line(), column());
    }

    void Interaction::repr(stream& io) const {
        print(io, "#interaction");
    }

    // Sequence

    const ValueClass Sequence::CLASS(Value::CLASS);

    const Type* Sequence::lazyType(Stack& ctx) {
        if (!_children.size()) return VOID;
        else return _children.back()->type(ctx);
    }

    Sequence::Sequence(const vector<Value*>& children, u32 line, u32 column,
                const ValueClass* vc):
        Value(line, column, vc), _children(children) {
    }

    Sequence::~Sequence() {
        for (Value* v : _children) if (v) delete v;
    }

    const vector<Value*>& Sequence::children() const {
        return _children;
    }

    void Sequence::append(Value* v) {
        _children.push(v);
    }

    void Sequence::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Sequence");
        for (Value* v : _children) v->format(io, level + 1);
    }

    Meta Sequence::fold(Stack& ctx) {
        Meta m;
        for (Value* v : _children) {
            m = v->fold(ctx);
            if (!m) break;
        }
        return m;
    }
    
    Location* Sequence::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        Location* loc = frame.none();
        for (Value* v : _children) loc = v->gen(ctx, gen, frame);
        return loc;
    }

    Value* Sequence::clone(Stack& ctx) const {
        vector<Value*> children;
        for (Value* v : _children) children.push(v->clone(ctx));
        return new Sequence(children, line(), column());
    }

    void Sequence::repr(stream& io) const {
        print(io, "(");
        for (u32 i = 0; i < _children.size(); i ++) {
            if (i > 0) print(io, " ");
            print(io, _children[i]);
        }
        print(io, ")");
    }
    
    void Sequence::explore(Explorer& e) {
        e.visit(this);
        for (Value* v : _children) v->explore(e);
    }

    // Program
    
    const ValueClass Program::CLASS(Value::CLASS);

    const Type* Program::lazyType(Stack& ctx) {
        if (!_children.size()) return VOID;
        else return _children.back()->type(ctx);
    }

    Program::Program(const vector<Value*>& children, u32 line, u32 column,
                const ValueClass* vc):
        Value(line, column, vc), _children(children) {
    }

    Program::~Program() {
        for (Value* v : _children) if (v) delete v;
    }

    const vector<Value*>& Program::children() const {
        return _children;
    }

    void Program::append(Value* v) {
        _children.push(v);
    }

    void Program::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Program");
        for (Value* v : _children) v->format(io, level + 1);
    }

    Meta Program::fold(Stack& ctx) {
        Meta m;
        for (Value* v : _children) {
            m = v->fold(ctx);
            if (!m) break;
        }
        return m;
    }
    
    Location* Program::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        Location* loc = frame.none();
        for (Value* v : _children) loc = v->gen(ctx, gen, frame);
        return loc;
    }

    Value* Program::clone(Stack& ctx) const {
        vector<Value*> children;
        for (Value* v : _children) children.push(v->clone(ctx));
        return new Program(children, line(), column());
    }

    void Program::repr(stream& io) const {
        print(io, "(");
        for (u32 i = 0; i < _children.size(); i ++) {
            if (i > 0) print(io, " ");
            print(io, _children[i]);
        }
        print(io, ")");
    }
    
    void Program::explore(Explorer& e) {
        e.visit(this);
        for (Value* v : _children) v->explore(e);
    }

    // Lambda

    const ValueClass Lambda::CLASS(Builtin::CLASS);

    const Type* Lambda::lazyType(Stack& ctx) {
        Stack& s = _ctx ? *_ctx : ctx;
        const Type* mt = _match->type(s);

        catchErrors();
        const Type* bt = _body->type(s);
        if (countErrors()) bt = ANY;
        discardErrors();

        if (_match->is<Define>() || _match->is<Autodefine>()) {
            return find<FunctionType>(mt, bt, vector<Constraint>({
                Constraint(mt)
            }));
        }
        else if (auto fr = _match->fold(s)) {
            return find<FunctionType>(mt, bt, vector<Constraint>({
                Constraint(fr)
            }));
        }
        else return find<FunctionType>(mt, bt);
    }
        
    Lambda::Lambda(u32 line, u32 column, const ValueClass* vc):
        Builtin(line, column, vc), _ctx(nullptr), _bodyscope(nullptr),
        _body(nullptr), _match(nullptr), _inlined(false) {
        setType(find<FunctionType>(ANY, ANY, true));
    }

    Lambda::~Lambda() {
        if (_match) delete _match;
        if (_body) delete _body;
    }

    bool Lambda::canApply(Stack& ctx, Value* arg) const {
        return !_match || !_body;
    }

    Value* Lambda::apply(Stack& ctx, Value* arg) {
        if (!_match) {
            _match = arg;
            
            Stack* self = new Stack(&ctx, true);
            Stack* args = new Stack(self, true);
            
            const Type* argt = nullptr;
            if (_match->is<Quote>()) {
                BlockTerm* b = _match->as<Quote>()->term()->as<BlockTerm>();
                if (b->children().size() > 1) {
                    if (!b->children()[0]->is<VariableTerm>()) {
                        auto t = b->children()[0];
                        err(PHASE_TYPE, t->line(), t->column(),
                            "Expected function name.");
                    }
                    else {
                        _name = b->children()[0]->as<VariableTerm>()->name();
                    }
                    b->children()[1]->eval(*args);
                }
                else b->eval(*args);
            }
            else args->push(_match);
            if (args->size() > 1) {
                err(PHASE_TYPE, line(), column(), 
                    "Too many match values provided in lambda ", 
                    "expression. Expected 1, but found ", args->size(),
                    ".");
            }
            else if (args->size() == 1) {
                if (args->top()->is<Variable>())
                    argt = ANY;
                else if (args->top()->is<Define>()) {
                    if (args->scope().find(args->top()->as<Define>()->name()) 
                        == args->scope().end())
                        args->top()->as<Define>()->apply(*args, nullptr);
                    argt = args->top()->type(*args);
                }
                else if (args->top()->fold(*args))
                    argt = args->top()->type(*args);
                else {
                    argt = ERROR;
                    err(PHASE_TYPE, args->top()->line(), 
                        args->top()->column(),
                        "Expected either definition or constant ",
                        "expression in match for lambda expression.");
                    note(PHASE_TYPE, args->top()->line(),
                         args->top()->column(),
                         "Found: ", args->top());
                }
                if (_match->is<Quote>()) delete _match;
                _match = args->top();
            }
            else if (args->size() == 0) {
                delete _match;
                _match = new Void(line(), column());
            }
            _ctx = args;
        }
        else if (!_body) {
            _body = arg;
            const Type* argt = _match->is<Variable>() ? ANY : _match->type(*_ctx);
            if (argt != ANY) {
                Stack* body = new Stack(_ctx);
                catchErrors();
                _body->as<Quote>()->term()->eval(*body);
                if (!countErrors()) {
                    vector<Value*> bodyvals;
                    for (Value* v : *body) bodyvals.push(v);
                    delete _body;
                    _body = bodyvals.size() == 1 ? bodyvals[0]
                        : new Sequence(bodyvals, line(), column());
                    updateType(ctx);
                    complete(ctx);
                }
                else {
                    const Type *mt = argt, *bt = ANY;
                    if (_match->is<Define>() || _match->is<Autodefine>()) {
                        setType(find<FunctionType>(mt, bt, vector<Constraint>({
                            Constraint(mt)
                        })));
                    }
                    else if (auto fr = _match->fold(*_ctx)) {
                        setType(find<FunctionType>(mt, bt, vector<Constraint>({
                            Constraint(fr)
                        })));
                    }
                    else setType(find<FunctionType>(mt, bt));
                }
                discardErrors();
                _bodyscope = body;
            }
            else setType(find<FunctionType>(ANY, ANY));

            if (_name.size() > 0) { // if name given
                Autodefine* def = new Autodefine(line(), column());
                Quote* name = new Quote(
                    new VariableTerm(_name, line(), column()),
                    line(), column()
                );
                def->apply(ctx, name);
                def->apply(ctx, this);
                return def;
            } 
        }
        return this;
    }

    Value* Lambda::body() {
        return _body;
    }

    Value* Lambda::match() {
        return _match;
    }

    Stack* Lambda::scope() {
        return _ctx;
    }

    Stack* Lambda::self() {
        return _ctx->parent();
    }

    class GatherVars : public Explorer {
    public:
        set<ustring> vars;
        virtual void visit(Value* v) override {
            if (v->is<Variable>()) vars.insert(v->as<Variable>()->name());
        }
    };

    void Lambda::complete(Stack& ctx) {
        const FunctionType* ft = this->type(*_ctx)->as<FunctionType>();
        if (ft->arg() != ANY && ft->ret() == ANY
            && _body->is<Quote>()) {
            Stack* body = _bodyscope;
            body->clear();
            _body->as<Quote>()->term()->eval(*body);
            vector<Value*> bodyvals;
            for (Value* v : *body) bodyvals.push(v);
            delete _body;
            _body = bodyvals.size() == 1 ? bodyvals[0]
                : new Sequence(bodyvals, line(), column());
            updateType(*_ctx);
        }

        GatherVars gatherer;
        _body->explore(gatherer);

        _captures = map<ustring, Stack::Entry>();
        for (const ustring& var : gatherer.vars) {
            const Stack* s = ctx.findenv(var);
            if (s && s->parent()
                && s->depth() < _ctx->depth()) 
                _captures.put(var, *(*s)[var]);
        }

        for (auto& p : _captures) {
            if (p.second.builtin()) 
                self()->bind(p.first, p.second.type(), p.second.builtin());
            else
                self()->bind(p.first, p.second.type());
            (*self())[p.first]->value() = p.second.value();
            (*self())[p.first]->storage() = STORAGE_CAPTURE;
        }

        // print(_stdout, (Value*)this, " closes on { ");
        // for (const ustring& s : _captures) {
        //     print(_stdout, s, " ");
        // }
        // println(_stdout, "}");
    }
        
    void Lambda::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Lambda");
        if (_match) _match->format(io, level + 1);
        if (_body) _body->format(io, level + 1);
    }

    Meta Lambda::fold(Stack& ctx) {
        if (!_match || !_body) return Meta();
        return Meta(type(ctx), new MetaFunction(this));
    }

    void Lambda::bindrec(const ustring& name, const Type* type,
                          const Meta& value) {
        if (!_match || !_body) return;
        scope()->name() = name + ".args";
        self()->name() = name + ".self";
        // self()->bind(name, type);
        // self()->operator[](name)->value() = value;
        complete(*_ctx);
    }
    
    Location* Lambda::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        if (type(ctx)->as<FunctionType>()->arg() == ANY) return frame.none();
        if (!_label.size()) {
            Function* func = _alts.size() ? gen.newFunction(_alts[0]) : gen.newFunction();
            _label = func->label();
            for (u32 i = 1; i < _alts.size(); i ++)
                func->add(new Label(_label, true));
            if (auto e = _match->entry(*_ctx)) {
                e->loc() = func->stack(_match->type(*_ctx));
                func->add(new MovInsn(e->loc(),
                    gen.locateArg(_match->type(*_ctx))));
            }
                // e->loc() = Location(STACK, 16, _match->type(*_ctx));
            Location* retval = _body->gen(*_ctx, gen, *func);
            if (type(ctx)->as<FunctionType>()->ret() != VOID) 
                func->add(new RetInsn(retval))->value(gen, *func);
        }
        Location* loc = frame.stack(type(ctx));
        frame.add(new LeaInsn(loc, _label));
        return loc;
    }

    bool Lambda::inlined() const {
        return _inlined;
    }
    
    Location* Lambda::genInline(Stack& ctx, Location* arg, CodeGenerator& gen, CodeFrame& frame) {
        _inlined = true;
        if (auto e = _match->entry(*_ctx)) {
            e->loc() = arg;
        }
        Location* retval = _body->gen(*_ctx, gen, frame);
        if (type(ctx)->as<FunctionType>()->ret() == VOID) 
            retval = frame.none();
        return retval;
    }

    const ustring& Lambda::label() const {
        return _label;
    }
    
    void Lambda::addAltLabel(const ustring& label) {
        _alts.push(label);
    }

    Value* Lambda::clone(Stack& ctx) const {
        Lambda* n = new Lambda(line(), column());
        if (_match) n->apply(ctx, _match->clone(ctx));
        if (_match && _body) n->apply(ctx, _body->clone(ctx));
        return n;
    }

    void Lambda::repr(stream& io) const {
        if (!_match && !_body) print(io, "(lambda ?? ??)");
        else if (!_body) print(io, "(lambda ", _match, " ??)");
        else print(io, "(lambda ", _match, " ", _body, ")");
    }
    
    void Lambda::explore(Explorer& e) {
        e.visit(this);
        if (_match) _match->explore(e);
        if (_body) _body->explore(e);
    }

    void Lambda::instantiate(const Type* t, Lambda* l) {
        insts.put(t, l);
    }

    Lambda* Lambda::instance(const Type* t) {
        auto it = insts.find(t);
        if (it != insts.end()) return it->second;
        return nullptr;
    }

    // Call
        
    const ValueClass Call::CLASS(Value::CLASS);

    Lambda* caseFor(Stack& ctx, Value* fn, Value* arg) {
        if (fn->is<Lambda>()) return fn->as<Lambda>();
        if (fn->is<Intersect>()) {
            Meta a = arg->fold(ctx);
            return fn->as<Intersect>()->caseFor(ctx, a);
        }
        return nullptr;
    }

    Lambda* instantiate(Stack& callctx, Lambda* l, const Type* at) {
        if (auto existing = l->instance(at)) return existing;
        ustring name = "";
        if (l->match()->is<Variable>()) name = l->match()->as<Variable>()->name();
        if (l->match()->is<Define>()) name = l->match()->as<Define>()->name();
        Lambda* n = new Lambda(l->line(), l->column());
        Define* arg = new Define(new TypeConstant(at, 0, 0), name);
        Stack* p = l->self()->parent();
        n->apply(*p, arg);
        n->apply(*p, l->body()->clone(*p));
        n->complete(callctx);
        l->instantiate(at, n);
        return n;
    }

    Lambda* instantiate(Stack& callctx, Lambda* l, Value* a) {
        const Type* at = a->type(callctx);
        if (auto existing = l->instance(at)) return existing;
        ustring name = "";
        if (l->match()->is<Variable>()) name = l->match()->as<Variable>()->name();
        if (l->match()->is<Define>()) name = l->match()->as<Define>()->name();
        Lambda* n = new Lambda(l->line(), l->column());
        Define* arg = new Define(new TypeConstant(at, 0, 0), name);
        Stack* p = l->self()->parent();
        n->apply(*p, arg);
        assign(*n->scope(), n->match(), a);
        n->apply(*p, l->body()->clone(*p));
        n->complete(callctx);
        l->instantiate(at, n);
        return n;
    }

    const Type* Call::lazyType(Stack& ctx) {
        if (_func->type(ctx)->is<FunctionType>()
            && _func->type(ctx)->as<FunctionType>()->arg() != ANY)
            return _func->type(ctx)->as<FunctionType>()->ret();

        Lambda* l = nullptr;
        Meta m = _func->fold(ctx);
        if (m.isFunction()) {
            l = caseFor(ctx, m.asFunction().value(), _arg);
        }
        else if (m.isIntersect()) {
            m = m.asIntersect().as(desiredfn);
            if (!m.isFunction()) {
                err(PHASE_TYPE, line(), column(),
                    "Called object '", _func, "' does not have function type.");
                return ERROR;
            }
            if (!m.type()->as<FunctionType>()->total()) {
                err(PHASE_TYPE, line(), column(),
                    "Cannot call ", m.type(), " case of ", _func->type(ctx), 
                    " intersect; cases are not total.");
                return ERROR;
            }
            l = caseFor(ctx, m.asFunction().value(), _arg);
        }
        else {
            err(PHASE_TYPE, line(), column(),
                "Called object '", _func, "' does not have function type.");
        }
        if (l->type(ctx)->as<FunctionType>()->arg() == ANY)
            inst = l = instantiate(ctx, l, _arg);

        return l->type(ctx)->as<FunctionType>()->ret();
    }

    Call::Call(Value* func, const Type* desired, Value* arg, u32 line, u32 column,
               const ValueClass* vc):
        Value(line, column, vc), _func(func), _arg(arg), 
        desiredfn(desired), inst(nullptr) {
        //
    }

    Call::Call(Value* func, const Type* desired, u32 line, u32 column,
               const ValueClass* vc):
        Call(func, desired, nullptr, line, column, vc) {
        //
    }

    Call::~Call() {
        delete _func;
        if (_arg) delete _arg;
    }

    void Call::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Call");
        _func->format(io, level + 1);
        if (_arg) _arg->format(io, level + 1);
    }

    Meta Call::fold(Stack& ctx) {
        if (inst) {
            Lambda* l = inst->as<Lambda>();
            assign(*l->scope(), l->match(), _arg);
            return l->body()->fold(*l->scope());
        }

        Lambda* l = nullptr;
        Meta m = _func->fold(ctx);
        if (m.isFunction()) {
            l = caseFor(ctx, m.asFunction().value(), _arg);
        }
        else if (m.isIntersect()) {
            m = m.asIntersect().as(desiredfn);
            if (!m.isFunction()) {
                err(PHASE_TYPE, line(), column(),
                    "Called object '", _func, "' does not have function type.");
                return ERROR;
            }
            if (!m.type()->as<FunctionType>()->total()) {
                err(PHASE_TYPE, line(), column(),
                    "Cannot call ", m.type(), " case of ", _func->type(ctx), 
                    " intersect; cases are not total.");
                return ERROR;
            }
            l = caseFor(ctx, m.asFunction().value(), _arg);
        }
        else {
            l = nullptr;
        }

        if (l) {
            // l->repr(_stdout);
            if (l->type(ctx)->as<FunctionType>()->arg() == ANY) {
                inst = l = instantiate(ctx, l, _arg);
            }
            auto backup = l->scope()->scope();
            assign(*l->scope(), l->match(), _arg);
            Meta m = l->body()->fold(*l->scope());
            l->scope()->scope() = backup;
            return m;
        }
        return Meta();
    }
    
    Location* Call::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        Location* fn = inst ? inst->gen(ctx, gen, frame)
            : _func->fold(ctx).asFunction().value()->gen(ctx, gen, frame);
        return frame.add(new CallInsn(_arg->gen(ctx, gen, frame), 
            fn))->value(gen, frame);
    }

    Value* Call::clone(Stack& ctx) const {
        return new Call(_func->clone(ctx), desiredfn, _arg->clone(ctx), line(), column());
    }

    void Call::repr(stream& io) const {
        if (inst) print(io, "(", (Value*)inst, " ", _arg, ")");
        else print(io, "(", _func, " ", _arg, ")");
    }
    
    void Call::explore(Explorer& e) {
        e.visit(this);
        if (_func) _func->explore(e);
        if (_arg) _arg->explore(e);
    }

    // BinaryOp

    const ValueClass BinaryOp::CLASS(Builtin::CLASS);

    BinaryOp::BinaryOp(const char* opname, u32 line, u32 column, 
                       const ValueClass* vc):
        Builtin(line, column, vc), _opname(opname), lhs(nullptr), rhs(nullptr) {
        //
    }

    BinaryOp::~BinaryOp() {
        if (lhs) delete lhs;
        if (rhs) delete rhs;
    }

    const Value* BinaryOp::left() const {
        return lhs;
    }

    const Value* BinaryOp::right() const {
        return rhs;
    }

    Value* BinaryOp::left() {
        return lhs;
    }

    Value* BinaryOp::right() {
        return rhs;
    }

    void BinaryOp::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, _opname);
        if (lhs) lhs->format(io, level + 1);
        if (rhs) rhs->format(io, level + 1);
    }

    Value* BinaryOp::apply(Stack& ctx, Value* arg) {
        if (!lhs) lhs = arg;
        else if (!rhs) rhs = arg;
        return this;
    }
    
    bool BinaryOp::canApply(Stack& ctx, Value* arg) const {
        return !lhs || !rhs;
    }

    void BinaryOp::repr(stream& io) const {
        if (!lhs && !rhs) print(io, _opname);
        else if (!rhs) print(io, "(", lhs, " ", _opname, ")");
        else print(io, "(", lhs, " ", _opname, " ", rhs, ")");
    }
    
    void BinaryOp::explore(Explorer& e) {
        e.visit(this);
        if (lhs) lhs->explore(e);
        if (rhs) rhs->explore(e);
    }

    // UnaryOp

    const ValueClass UnaryOp::CLASS(Builtin::CLASS);

    UnaryOp::UnaryOp(const char* opname, u32 line, u32 column, 
                const ValueClass* vc):
        Builtin(line, column, vc), _opname(opname), _operand(nullptr) {
        //
    }

    UnaryOp::~UnaryOp() {
        if (_operand) delete _operand;
    }

    const Value* UnaryOp::operand() const {
        return _operand;
    }

    Value* UnaryOp::operand() {
        return _operand;
    }

    Value* UnaryOp::apply(Stack& ctx, Value* arg) {
        if (!_operand) {
            _operand = arg;
        }
        return this;
    }

    void UnaryOp::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, _opname);
        if (_operand) _operand->format(io, level + 1);
    }

    bool UnaryOp::canApply(Stack& ctx, Value* arg) const {
        return !_operand;
    }

    void UnaryOp::repr(stream& io) const {
        if (!_operand) print(io, _opname);
        else print(io, "(", _opname, " ", _operand, ")");
    }
    
    void UnaryOp::explore(Explorer& e) {
        e.visit(this);
        if (_operand) _operand->explore(e);
    }

    // BinaryMath

    const ValueClass BinaryMath::CLASS(BinaryOp::CLASS);

    const Type *BinaryMath::_BASE_TYPE, 
               *BinaryMath::_PARTIAL_INT, 
               *BinaryMath::_PARTIAL_DOUBLE;

    const Type* BinaryMath::BASE_TYPE() {
        if (!_BASE_TYPE) _BASE_TYPE = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(I64, 
                find<IntersectionType>(set<const Type*>({
                    find<FunctionType>(I64, I64),
                    find<FunctionType>(DOUBLE, DOUBLE)
                }))
            ),
            find<FunctionType>(DOUBLE, find<FunctionType>(DOUBLE, DOUBLE))
        }));
        return _BASE_TYPE;
    }

    const Type* BinaryMath::PARTIAL_INT_TYPE() { 
        if (!_PARTIAL_INT) {
            _PARTIAL_INT = find<IntersectionType>(set<const Type*>({
                find<FunctionType>(I64, I64),
                find<FunctionType>(DOUBLE, DOUBLE)
            }));
        }
        return _PARTIAL_INT;
    }

    const Type* BinaryMath::PARTIAL_DOUBLE_TYPE() {
        if (!_PARTIAL_DOUBLE) {
            _PARTIAL_DOUBLE = find<FunctionType>(DOUBLE, DOUBLE);
        }
        return _PARTIAL_DOUBLE;
    }

    BinaryMath::BinaryMath(const char* opname, u32 line, u32 column,
                           const ValueClass* vc):
        BinaryOp(opname, line, column, vc) {
        setType(BASE_TYPE());
    }

    Value* BinaryMath::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            if (lhs->type(ctx) == I64) setType(PARTIAL_INT_TYPE());
            else if (lhs->type(ctx) == DOUBLE) setType(PARTIAL_DOUBLE_TYPE());
        }
        else if (!rhs) {
            rhs = arg;

            // join types if necessary
            if (rhs->type(ctx) != lhs->type(ctx)) {
                const Type* j = join(rhs->type(ctx), lhs->type(ctx));
                if (rhs->type(ctx) != j) rhs = new Cast(j, rhs);
                else if (lhs->type(ctx) != j) lhs = new Cast(j, lhs);
            }
            setType(lhs->type(ctx));
        }
        return this;
    }

    // Add

    const Type* Add::_BASE_TYPE;

    const Type* Add::BASE_TYPE() {
        if (!_BASE_TYPE) _BASE_TYPE = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(I64, 
                find<IntersectionType>(set<const Type*>({
                    find<FunctionType>(I64, I64),
                    find<FunctionType>(DOUBLE, DOUBLE)
                }))
            ),
            find<FunctionType>(DOUBLE, find<FunctionType>(DOUBLE, DOUBLE)),
            find<FunctionType>(STRING, find<FunctionType>(STRING, STRING))
        }));
        return _BASE_TYPE;
    }

    const ValueClass Add::CLASS(BinaryMath::CLASS);

    Add::Add(u32 line, u32 column, const ValueClass* vc):
        BinaryMath("+", line, column, vc) {
        //
    }

    Value* Add::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            if (lhs->type(ctx) == I64) 
                setType(PARTIAL_INT_TYPE());
            else if (lhs->type(ctx) == DOUBLE) 
                setType(PARTIAL_DOUBLE_TYPE());
            else if (lhs->type(ctx) == STRING) 
                setType(find<FunctionType>(STRING, STRING));
        }
        else BinaryMath::apply(ctx, arg);
        return this;
    }

    Meta Add::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return add(lhs->fold(ctx), rhs->fold(ctx));
    }

    Location* Add::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        if (lhs->type(ctx) == STRING) {
            // concat
            return frame.add(new CCallInsn(
                { lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame) },
                "_strcat", STRING
            ))->value(gen, frame);
        }
        return frame.add(
            new AddInsn(lhs->gen(ctx, gen, frame), 
                rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* Add::clone(Stack& ctx) const {
        Add* node = new Add(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }  

    // Subtract

    const ValueClass Subtract::CLASS(BinaryMath::CLASS);

    Subtract::Subtract(u32 line, u32 column, const ValueClass* vc):
        BinaryMath("-", line, column, vc) {
        //
    }

    Meta Subtract::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return sub(lhs->fold(ctx), rhs->fold(ctx));
    }

    Location* Subtract::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new SubInsn(lhs->gen(ctx, gen, frame), 
                rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* Subtract::clone(Stack& ctx) const {
        Subtract* node = new Subtract(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }  

    // Multiply

    const ValueClass Multiply::CLASS(BinaryMath::CLASS);

    Multiply::Multiply(u32 line, u32 column, const ValueClass* vc):
        BinaryMath("*", line, column, vc) {
        //
    }

    Meta Multiply::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return mul(lhs->fold(ctx), rhs->fold(ctx));
    }

    Location* Multiply::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new MulInsn(lhs->gen(ctx, gen, frame), 
                rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* Multiply::clone(Stack& ctx) const {
        Multiply* node = new Multiply(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    // Divide

    const ValueClass Divide::CLASS(BinaryMath::CLASS);

    Divide::Divide(u32 line, u32 column, const ValueClass* vc):
        BinaryMath("/", line, column, vc) {
        //
    }

    Meta Divide::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return div(lhs->fold(ctx), rhs->fold(ctx));
    }

    Location* Divide::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new DivInsn(lhs->gen(ctx, gen, frame), 
                rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* Divide::clone(Stack& ctx) const {
        Divide* node = new Divide(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    // Modulus

    const ValueClass Modulus::CLASS(BinaryMath::CLASS);

    Modulus::Modulus(u32 line, u32 column, const ValueClass* vc):
        BinaryMath("%", line, column, vc) {
        //
    }

    Meta Modulus::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return mod(lhs->fold(ctx), rhs->fold(ctx));
    }

    Location* Modulus::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new ModInsn(lhs->gen(ctx, gen, frame), 
                rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* Modulus::clone(Stack& ctx) const {
        Modulus* node = new Modulus(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    // BinaryLogic

    const Type *BinaryLogic::_BASE_TYPE, 
               *BinaryLogic::_PARTIAL_BOOL;

    const Type *BinaryLogic::BASE_TYPE() {
        if (_BASE_TYPE) return _BASE_TYPE;
        return _BASE_TYPE = find<FunctionType>(BOOL, 
            find<FunctionType>(BOOL, BOOL)
        );
    }

    const Type *BinaryLogic::PARTIAL_BOOL_TYPE() {
        if (_PARTIAL_BOOL) return _PARTIAL_BOOL;
        return _PARTIAL_BOOL = find<FunctionType>(BOOL, BOOL);
    }

    const ValueClass BinaryLogic::CLASS(BinaryOp::CLASS);

    BinaryLogic::BinaryLogic(const char* opname, u32 line, u32 column,
                const ValueClass* vc):
        BinaryOp(opname, line, column, vc) {
        setType(_BASE_TYPE);
    }

    Value* BinaryLogic::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            setType(PARTIAL_BOOL_TYPE());
        }
        else if (!rhs) {
            rhs = arg;
            setType(BOOL);
        }
        return this;
    }

    // And

    const ValueClass And::CLASS(BinaryLogic::CLASS);

    And::And(u32 line, u32 column, const ValueClass* vc):
        BinaryLogic("and", line, column, vc) {
        //
    }

    Meta And::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return andf(lhs->fold(ctx), rhs->fold(ctx));
    }

    Location* And::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new AndInsn(lhs->gen(ctx, gen, frame), 
                rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* And::clone(Stack& ctx) const {
        And* node = new And(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    // Or

    const ValueClass Or::CLASS(BinaryLogic::CLASS);

    Or::Or(u32 line, u32 column, const ValueClass* vc):
        BinaryLogic("or", line, column, vc) {
        //
    }

    Meta Or::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return orf(lhs->fold(ctx), rhs->fold(ctx));
    }

    Location* Or::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new OrInsn(lhs->gen(ctx, gen, frame), 
                rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* Or::clone(Stack& ctx) const {
        Or* node = new Or(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    // Xor

    const ValueClass Xor::CLASS(BinaryLogic::CLASS);

    Xor::Xor(u32 line, u32 column, const ValueClass* vc):
        BinaryLogic("xor", line, column, vc) {
        //
    }

    Meta Xor::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return xorf(lhs->fold(ctx), rhs->fold(ctx));
    }

    Location* Xor::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new XorInsn(lhs->gen(ctx, gen, frame), 
                rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* Xor::clone(Stack& ctx) const {
        Xor* node = new Xor(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    // Not
        
    const ValueClass Not::CLASS(UnaryOp::CLASS);

    Not::Not(u32 line, u32 column, const ValueClass* vc):
        UnaryOp("not", line, column, vc) {
        setType(find<FunctionType>(BOOL, BOOL));
    }

    Value* Not::apply(Stack& ctx, Value* arg) {
        if (!_operand) {
            _operand = arg;
            setType(BOOL);
        }
        return this;
    }

    Meta Not::fold(Stack& ctx) {
        if (!_operand) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return notf(_operand->fold(ctx));
    }

    Location* Not::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new NotInsn(_operand->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    Value* Not::clone(Stack& ctx) const {
        Not* node = new Not(line(), column());
        if (_operand) node->apply(ctx, _operand);
        return node;
    }

    // BinaryEquality

    const Type *BinaryEquality::_BASE_TYPE, 
               *BinaryEquality::_PARTIAL_INT, 
               *BinaryEquality::_PARTIAL_DOUBLE, 
               *BinaryEquality::_PARTIAL_BOOL;

    const Type *BinaryEquality::BASE_TYPE() {
        if (_BASE_TYPE) return _BASE_TYPE;
        return _BASE_TYPE = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(I64, 
                find<IntersectionType>(set<const Type*>({
                    find<FunctionType>(I64, BOOL),
                    find<FunctionType>(DOUBLE, BOOL)
                }))
            ),
            find<FunctionType>(BOOL, find<FunctionType>(BOOL, BOOL)),
            find<FunctionType>(STRING, find<FunctionType>(STRING, BOOL)),
            find<FunctionType>(TYPE, find<FunctionType>(TYPE, BOOL)),
            find<FunctionType>(DOUBLE, find<FunctionType>(DOUBLE, BOOL))
        }));
    }

    const Type *BinaryEquality::PARTIAL_INT_TYPE() {
        if (_PARTIAL_INT) return _PARTIAL_INT;
        return _PARTIAL_INT = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(I64, BOOL),
            find<FunctionType>(DOUBLE, BOOL)
        }));
    }

    const Type *BinaryEquality::PARTIAL_DOUBLE_TYPE() {
        if (_PARTIAL_DOUBLE) return _PARTIAL_DOUBLE;
        return _PARTIAL_DOUBLE = find<FunctionType>(DOUBLE, BOOL);
    }

    const Type *BinaryEquality::PARTIAL_BOOL_TYPE() {
        if (_PARTIAL_BOOL) return _PARTIAL_BOOL;
        return _PARTIAL_BOOL = find<FunctionType>(BOOL, BOOL);
    }    

    const ValueClass BinaryEquality::CLASS(BinaryOp::CLASS);

    BinaryEquality::BinaryEquality(const char* opname, u32 line, u32 column,
                    const ValueClass* vc):
        BinaryOp(opname, line, column, vc) {
        setType(BASE_TYPE());
    }

    Value* BinaryEquality::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            if (lhs->type(ctx) == I64) setType(PARTIAL_INT_TYPE());
            else if (lhs->type(ctx) == BOOL) setType(PARTIAL_BOOL_TYPE());
            else if (lhs->type(ctx) == DOUBLE) setType(PARTIAL_DOUBLE_TYPE());
        }
        else if (!rhs) {
            rhs = arg;
            setType(BOOL);
        }
        return this;
    }

    // Equal

    const ValueClass Equal::CLASS(BinaryEquality::CLASS);

    Equal::Equal(u32 line, u32 column, const ValueClass* vc):
        BinaryEquality("==", line, column, vc) {
        //
    }

    Meta Equal::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return equal(lhs->fold(ctx), rhs->fold(ctx));
    }

    Value* Equal::clone(Stack& ctx) const {
        Equal* node = new Equal(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    Location* Equal::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new EqInsn(lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    // Inequal

    const ValueClass Inequal::CLASS(BinaryEquality::CLASS);

    Inequal::Inequal(u32 line, u32 column, const ValueClass* vc):
        BinaryEquality("!=", line, column, vc) {
        //
    }

    Meta Inequal::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return inequal(lhs->fold(ctx), rhs->fold(ctx));
    }

    Value* Inequal::clone(Stack& ctx) const {
        Inequal* node = new Inequal(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    Location* Inequal::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new NotEqInsn(lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    // BinaryRelation

    const Type *BinaryRelation::_BASE_TYPE, 
               *BinaryRelation::_PARTIAL_INT, 
               *BinaryRelation::_PARTIAL_DOUBLE;

    const Type *BinaryRelation::BASE_TYPE() {
        if (_BASE_TYPE) return _BASE_TYPE;
        return _BASE_TYPE = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(I64, 
                find<IntersectionType>(set<const Type*>({
                    find<FunctionType>(I64, BOOL),
                    find<FunctionType>(DOUBLE, BOOL)
                }))
            ),
            find<FunctionType>(DOUBLE, find<FunctionType>(DOUBLE, BOOL)),
            find<FunctionType>(STRING, find<FunctionType>(STRING, BOOL))
        }));
    }

    const Type *BinaryRelation::PARTIAL_INT_TYPE() {
        if (_PARTIAL_INT) return _PARTIAL_INT;
        return _PARTIAL_INT = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(I64, BOOL),
            find<FunctionType>(DOUBLE, BOOL)
        }));
    }

    const Type *BinaryRelation::PARTIAL_DOUBLE_TYPE() {
        if (_PARTIAL_DOUBLE) return _PARTIAL_DOUBLE;
        return _PARTIAL_DOUBLE = find<FunctionType>(DOUBLE, BOOL);
    } 

    const ValueClass BinaryRelation::CLASS(BinaryOp::CLASS);

    BinaryRelation::BinaryRelation(const char* opname, u32 line, u32 column,
                    const ValueClass* vc):
        BinaryOp(opname, line, column, vc) {
        setType(BASE_TYPE());
    }

    Value* BinaryRelation::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            if (lhs->type(ctx) == I64) setType(PARTIAL_INT_TYPE());
            else if (lhs->type(ctx) == DOUBLE) setType(PARTIAL_DOUBLE_TYPE());
        }
        else if (!rhs) {
            rhs = arg;
            setType(BOOL);
        }
        return this;
    }

    // Less

    const ValueClass Less::CLASS(BinaryRelation::CLASS);

    Less::Less(u32 line, u32 column, const ValueClass* vc):
        BinaryRelation("<", line, column, vc) {
        //
    }

    Meta Less::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return less(lhs->fold(ctx), rhs->fold(ctx));
    }

    Value* Less::clone(Stack& ctx) const {
        Less* node = new Less(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    Location* Less::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new LessInsn(lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    // LessEqual

    const ValueClass LessEqual::CLASS(BinaryRelation::CLASS);

    LessEqual::LessEqual(u32 line, u32 column, const ValueClass* vc):
        BinaryRelation("<=", line, column, vc) {
        //
    }

    Meta LessEqual::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return lessequal(lhs->fold(ctx), rhs->fold(ctx));
    }

    Value* LessEqual::clone(Stack& ctx) const {
        LessEqual* node = new LessEqual(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    Location* LessEqual::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new LessEqInsn(lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    // Greater

    const ValueClass Greater::CLASS(BinaryRelation::CLASS);

    Greater::Greater(u32 line, u32 column, const ValueClass* vc):
        BinaryRelation(">", line, column, vc) {
        //
    }

    Meta Greater::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return greater(lhs->fold(ctx), rhs->fold(ctx));
    }

    Value* Greater::clone(Stack& ctx) const {
        Greater* node = new Greater(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    Location* Greater::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new GreaterInsn(lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    // GreaterEqual

    const ValueClass GreaterEqual::CLASS(BinaryRelation::CLASS);

    GreaterEqual::GreaterEqual(u32 line, u32 column, const ValueClass* vc):
        BinaryRelation(">=", line, column, vc) {
        //
    }

    Meta GreaterEqual::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(find<FunctionType>(ANY, ANY), new MetaFunction(this));
        return greaterequal(lhs->fold(ctx), rhs->fold(ctx));
    }

    Value* GreaterEqual::clone(Stack& ctx) const {
        GreaterEqual* node = new GreaterEqual(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    Location* GreaterEqual::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(
            new GreaterEqInsn(lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame))
        )->value(gen, frame);
    }

    // ArrayDef
    
    const ValueClass ArrayDef::CLASS(Builtin::CLASS);

    ArrayDef::ArrayDef(u32 line, u32 column, 
                const ValueClass* vc):
        Builtin(line, column, vc), _type(nullptr), _dim(nullptr) {
        setType(find<IntersectionType>(set<const Type*>({
            find<FunctionType>(TYPE, find<FunctionType>(find<ArrayType>(ANY), TYPE)),
            find<FunctionType>(find<ArrayType>(ANY), find<FunctionType>(TYPE, TYPE)),
        })));
    }

    ArrayDef::~ArrayDef() {
        if (_type) delete _type;
        if (_dim) delete _dim;
    }

    void ArrayDef::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "ArrayDef");
        if (_type) _type->format(io, level + 1);
        if (_dim) _dim->format(io, level + 1);
    }

    Value* ArrayDef::apply(Stack& ctx, Value* v) {
        if (!_type || !_dim) {
            if (v->type(ctx) == TYPE) {
                _type = v;
                Meta m = _type->fold(ctx);
                if (!m.isType()) {
                    err(PHASE_TYPE, _type->line(), _type->column(),
                        "Cannot resolve array element type at compile ",
                        "time."
                    );
                    setType(ERROR);
                    return this;
                }
                setType(_dim ? TYPE : find<FunctionType>(find<ArrayType>(ANY), TYPE));
            }
            else if (v->type(ctx)->is<ArrayType>()) {
                _dim = v;
                Meta m = _dim->fold(ctx);
                if (!m.isArray()) {
                    err(PHASE_TYPE, _dim->line(), _dim->column(),
                        "Cannot resolve array dimensions at compile ",
                        "time."
                    );
                    setType(ERROR);
                    return this;
                }
                MetaArray& a = m.asArray();
                for (const Meta& m : a) {
                    if (!m.isInt()) {
                        err(PHASE_TYPE, _dim->line(), _dim->column(),
                            "Array dimension is not an integer.");
                        setType(ERROR);
                        return this;
                    }
                    if (m.isInt() && m.asInt() < 0) {
                        err(PHASE_TYPE, _dim->line(), _dim->column(),
                            "Array dimension cannot be negative.");
                        setType(ERROR);
                        return this;
                    }
                }
                setType(_type ? TYPE : find<FunctionType>(TYPE, TYPE));
            }
            else {
                err(PHASE_TYPE, _dim->line(), _dim->column(),
                    "Unknown value in array type.");
                setType(ERROR);
            }
        }
        return this;
    }

    Meta ArrayDef::fold(Stack& ctx) {
        if (!_type || !_dim) return Meta(type(ctx), new MetaFunction(this));
        const Type* t = _type->fold(ctx).asType();
        MetaArray d = _dim->fold(ctx).asArray();
        if (!d.size()) return Meta(type(ctx), find<ArrayType>(t));
        i64 size = d[0].asInt();
        return Meta(type(ctx), find<ArrayType>(t, size));
    }

    Value* ArrayDef::clone(Stack& ctx) const {
        ArrayDef* a = new ArrayDef(line(), column());
        if (_type) a->apply(ctx, _type->clone(ctx));
        if (_dim) a->apply(ctx, _dim->clone(ctx));
        return a;
    }

    void ArrayDef::repr(stream& io) const {
        if (!_type) print(io, "??[??]");
        else if (!_dim) print(io, _type, "[??]");
        else print(io, _type, _dim);
    }

    // Array

    const ValueClass Array::CLASS(Builtin::CLASS);

    Array::Array(u32 line, u32 column, 
            const ValueClass* vc):
        Builtin(line, column, vc) {
        setType(find<FunctionType>(ANY, ANY, true));
    }

    Array::~Array() {
        for (Value* v : elts) delete v;
    }

    void Array::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Array");
        for (Value* v : elts) {
            v->format(io, level + 1);
        }
    }

    Value* Array::apply(Stack& ctx, Value* v) {
        Stack* tmp = new Stack(&ctx, false);
        v->as<Quote>()->term()->eval(*tmp);
        for (Value* v : *tmp) elts.push(v);
        vector<const Type*> ts;
        for (Value* v : elts) ts.push(v->type(ctx));
        setType(find<ArrayType>(unionOf(ts), elts.size()));
        return this;
    }

    Meta Array::fold(Stack& ctx) {
        if (type(ctx)->is<FunctionType>()) 
            return Meta(type(ctx), new MetaFunction(this));
        
        vector<Meta> metas;
        const ArrayType* mytype = type(ctx)->as<ArrayType>();
        for (Value* v : elts) {
            if (v->type(ctx) != mytype->element())
                metas.push(Meta(mytype->element(), new MetaUnion(v->fold(ctx))));
            else metas.push(v->fold(ctx));
        }
        for (Meta& m : metas) if (!m) return Meta();
        return Meta(type(ctx), new MetaArray(metas));
    }

    Value* Array::clone(Stack& ctx) const {
        Array* a = new Array(line(), column());
        for (Value* v : elts) a->elts.push(v->clone(ctx));
        return a;
    }

    void Array::repr(stream& io) const {
        print(io, "[");
        for (u32 i = 0; i < elts.size(); i ++) {
            if (i > 0) print(io, " ");
            elts[i]->repr(io);
        }
        print(io, "]");
    }

    bool Array::lvalue(Stack& ctx) {
        if (elts.size() == 0) return false;
        return type(ctx)->as<ArrayType>()->element()->is<ReferenceType>();
    }

    // Index

    const ValueClass Index::CLASS(Builtin::CLASS);

    Index::Index(u32 line, u32 column, 
            const ValueClass* vc):
        Builtin(line, column, vc), arr(nullptr), idx(nullptr) {
        setType(find<FunctionType>(
            find<ArrayType>(ANY),
            find<FunctionType>(find<ArrayType>(I64), ANY)
        ));
    }

    Index::~Index() {
        if (arr) delete arr;
        if (idx) delete idx;
    }

    void Index::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Index");
        if (arr) arr->format(io, level + 1);
        if (idx) idx->format(io, level + 1);
    }

    Value* Index::apply(Stack& ctx, Value* v) {
        if (!arr) {
            arr = v;
            setType(find<FunctionType>(find<ArrayType>(ANY), 
                find<ArrayType>(arr->type(ctx)->as<ArrayType>()->element())));
        }
        else if (!idx) {
            idx = v;
            const ArrayType* a = idx->type(ctx)->as<ArrayType>();
            const Type* elt = arr->type(ctx)->as<ArrayType>()->element();
            if (a->count() == 1)
                setType(find<ReferenceType>(elt));
            else
                setType(find<ArrayType>(find<ReferenceType>(elt), a->count()));
        }
        return this;
    }

    Meta Index::fold(Stack& ctx) {
        if (!arr || !idx) 
            return Meta(type(ctx), new MetaFunction(this));
        
        Meta a = arr->fold(ctx);
        Meta i = idx->fold(ctx);
        if (!a || !i) return Meta();
        
        if (i.asArray().size() == 1) {
            return Meta(type(ctx), a.asArray()[i.asArray()[0].asInt()]);
        }
        else {
            vector<Meta> ms;
            for (const Meta& m : i.asArray())
                ms.push(Meta(type(ctx), a.asArray()[m.asInt()]));
            return Meta(type(ctx), new MetaArray(ms));
        }
    }

    Value* Index::clone(Stack& ctx) const {
        Index* i = new Index(line(), column());
        if (arr) i->apply(ctx, arr->clone(ctx));
        if (idx) i->apply(ctx, idx->clone(ctx));
        return i;
    }

    void Index::repr(stream& io) const {
        if (!arr) print(io, "??[??]");
        else if (!idx) print(io, arr, "[??]");
        else print(io, arr, idx);
    }

    bool Index::lvalue(Stack& ctx) {
        return arr && idx;
    }

    // Range

    const ValueClass Range::CLASS(BinaryOp::CLASS);

    Range::Range(u32 line, u32 column, const ValueClass* vc):
        BinaryOp("..", line, column, vc) {
        setType(find<FunctionType>(I64, find<FunctionType>(I64, find<ArrayType>(I64))));
    }

    Meta Range::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(type(ctx), new MetaFunction(this));
        return Meta();
    }

    Value* Range::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            setType(find<FunctionType>(I64, find<ArrayType>(I64)));
        }
        else if (!rhs) {
            rhs = arg;
            Meta l = lhs->fold(ctx), r = rhs->fold(ctx);
            if (l && r) { // size known at compile time
                for (i64 i = l.asInt(); i <= r.asInt(); i ++)
                    ctx.push(new IntegerConstant(i, line(), column()));
            }
            else {
                err(PHASE_TYPE, line(), column(),
                    "Bounds of range expression must be constant.");
            }
            delete this;
            return nullptr;
        }
        return this;
    }

    Value* Range::clone(Stack& ctx) const {
        Range* r = new Range(line(), column());
        if (lhs) r->apply(ctx, lhs->clone(ctx));
        return r;
    }

    // Repeat

    const ValueClass Repeat::CLASS(BinaryOp::CLASS);

    Repeat::Repeat(u32 line, u32 column, const ValueClass* vc):
        BinaryOp("**", line, column) {
        setType(find<FunctionType>(ANY, find<FunctionType>(I64, ANY)));
    }
    
    Meta Repeat::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(type(ctx), new MetaFunction(this));
        return Meta();
    }
    
    Value* Repeat::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            setType(find<FunctionType>(I64, ANY));
        }
        else if (!rhs) {
            Meta m = arg->fold(ctx);
            if (!m.isInt()) {
                err(PHASE_TYPE, arg->line(), arg->column(),
                    "Number of repetitions must be constant integer.");
                delete this;
                return nullptr;
            }
            for (int i = 0; i < m.asInt(); i ++) {
                ctx.push(lhs->clone(ctx));
            }
            delete arg;
            delete this;
            return nullptr;
        }
        return this;
    }
    
    Value* Repeat::clone(Stack& ctx) const {
        Repeat* r = new Repeat(line(), column());
        if (lhs) r->apply(ctx, lhs->clone(ctx));
        return r;
    }

    // Cons

    const ValueClass Cons::CLASS(BinaryOp::CLASS);

    Cons::Cons(u32 line, u32 column, const ValueClass* vc):
        BinaryOp("Cons", line, column, vc) {
        setType(find<FunctionType>(ANY, find<FunctionType>(ANY, ANY)));
    }

    Value* Cons::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            const ListType* lt = find<ListType>(lhs->type(ctx));
            setType(find<IntersectionType>(set<const Type*>({
                find<FunctionType>(lt, lt),
                find<FunctionType>(EMPTY, lt)
            }))); 
        }
        else if (!rhs) {
            rhs = arg;
            setType(find<ListType>(lhs->type(ctx)));
        }
        return this;
    }

    Meta Cons::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(type(ctx), new MetaFunction(this));
        return cons(lhs->fold(ctx), rhs->fold(ctx));
    }

    Value* Cons::clone(Stack& ctx) const {
        Cons* node = new Cons(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    // Reference

    const ValueClass Reference::CLASS(UnaryOp::CLASS);

    Reference::Reference(u32 line, u32 column, const ValueClass* vc):
        UnaryOp("~", line, column, vc) {
        setType(find<FunctionType>(ANY, ANY));
    }

    Value* Reference::apply(Stack& ctx, Value* arg) {
        if (!_operand) {
            if (!arg->lvalue(ctx)) {
                err(PHASE_TYPE, line(), column(),
                    "Cannot take reference to non-lvalue.");
                setType(ERROR);
            }
            else _operand = arg,
                setType(find<ReferenceType>(_operand->type(ctx)));
        }
        return this;
    }

    Value* Reference::clone(Stack& ctx) const {
        Reference* ref = new Reference(line(), column());
        if (_operand) ref->apply(ctx, _operand);
        return ref;
    }

    Meta Reference::fold(Stack& ctx) {
        if (!_operand) return Meta();
        auto e = _operand->entry(ctx);
        if (e) return Meta(type(ctx), (Meta&)e->value());
        return Meta();
    }
    
    bool Reference::lvalue(Stack& ctx) {
        return true;
    }

    // Join

    const Type* Join::_BASE_TYPE;

    const Type* Join::BASE_TYPE() {
        if (_BASE_TYPE) return _BASE_TYPE;
        return _BASE_TYPE = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(TYPE, find<FunctionType>(TYPE, TYPE)),
            find<FunctionType>(ANY, find<FunctionType>(ANY, ANY))
        }));
    }

    const ValueClass Join::CLASS(BinaryOp::CLASS);

    Join::Join(u32 line, u32 column, const ValueClass* vc):
        BinaryOp(",", line, column, vc) {
        setType(BASE_TYPE());
    }

    Value* Join::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            if (lhs->type(ctx) == TYPE) setType(find<FunctionType>(TYPE, TYPE));
            else setType(find<FunctionType>(ANY, ANY));
        }
        else if (!rhs) {
            rhs = arg;

            setType(find<TupleType>(vector<const Type*>({
                lhs->type(ctx), rhs->type(ctx)
            })));
        }
        return this;
    }

    Meta Join::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta(type(ctx), new MetaFunction(this));
        return join(lhs->fold(ctx), rhs->fold(ctx));
    }
    
    bool Join::lvalue(Stack& ctx) {
        return lhs->lvalue(ctx) && rhs->lvalue(ctx);
    }

    Value* Join::clone(Stack& ctx) const {
        Join* node = new Join(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }
    
    Location* Join::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(new JoinInsn(
            { lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame) },
            type(ctx))
        )->value(gen, frame);
    }

    void Join::repr(stream& io) const {
        if (!lhs || !rhs) BinaryOp::repr(io);
        else print(io, "(", lhs, ", ", rhs, ")");
    }

    // Intersect

    const Type* Intersect::_BASE_TYPE;

    const Type* Intersect::BASE_TYPE() {
        if (_BASE_TYPE) return _BASE_TYPE;
        return _BASE_TYPE = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(TYPE, find<FunctionType>(TYPE, TYPE)),
            find<FunctionType>(ANY, find<FunctionType>(ANY, ANY))
        }));
    }

    const ValueClass Intersect::CLASS(BinaryOp::CLASS);

    const Type* Intersect::lazyType(Stack& ctx) {
        if (!lhs && !rhs) return _BASE_TYPE;
        else if (!rhs) return find<FunctionType>(ANY, ANY);

        const Type* lt = lhs->type(ctx);
        const Type* rt = rhs->type(ctx);
        
        if (lt->is<FunctionType>() && rt->is<FunctionType>()) {
            const FunctionType* lf = lt->as<FunctionType>();
            const FunctionType* rf = rt->as<FunctionType>();
            if (lf->arg() == rf->arg() 
                && (lf->ret()->explicitly(rf->ret())
                    || rf->ret()->explicitly(lf->ret()) 
                    || lf->ret() == ANY 
                    || rf->ret() == ANY)
                && !lf->conflictsWith(rf) && !rf->conflictsWith(lf)) {
                vector<Constraint> constraints;
                for (auto& c : lf->constraints()) constraints.push(c);
                for (auto& c : rf->constraints()) constraints.push(c);
                const Type* ret = join(lf->ret(), rf->ret());
                if (ret == ANY) 
                    ret = lf->ret() == ANY ? rf->ret() : lf->ret();
                return find<FunctionType>(lf->arg(), ret, constraints);
            }
            else {
                return find<IntersectionType>(set<const Type*>({ 
                    lt, rt 
                }));
            }
        }
        else {
            return find<IntersectionType>(set<const Type*>({ lt, rt }));
        }
    }

    void Intersect::populate(Stack& ctx, map<const Type*, 
                             vector<Value*>>& values) {
        if (lhs) {
            if (lhs->is<Intersect>()) {
                lhs->as<Intersect>()->populate(ctx, values);
            }
            else if (lhs->type(ctx)->is<FunctionType>()) {
                const FunctionType* ft = lhs->type(ctx)->as<FunctionType>();
                values[ft->arg()].push(lhs);
            }
            else values[nullptr].push(lhs);
            lhs = nullptr;
        }
        if (rhs) {
            if (rhs->is<Intersect>()) {
                rhs->as<Intersect>()->populate(ctx, values);
            }
            else if (rhs->type(ctx)->is<FunctionType>()) {
                const FunctionType* ft = rhs->type(ctx)->as<FunctionType>();
                values[ft->arg()].push(rhs);
            }
            else values[nullptr].push(rhs);
            rhs = nullptr;
        }
        delete this;
    }
    
    void Intersect::getFunctions(Stack& ctx, vector<Lambda*>& functions) {
        if (lhs) {
            if (lhs->type(ctx)->is<FunctionType>()) {
                Meta fr = lhs->fold(ctx);
                if (fr.isFunction() && fr.asFunction().value()->is<Lambda>())
                    functions.push(fr.asFunction().value()->as<Lambda>());
            }
            if (lhs->is<Intersect>()) 
                lhs->as<Intersect>()->getFunctions(ctx, functions);
        }
        if (rhs) {
            if (lhs->type(ctx)->is<FunctionType>()) {
                Meta fr = rhs->fold(ctx);
                if (fr.isFunction() && fr.asFunction().value()->is<Lambda>())
                    functions.push(fr.asFunction().value()->as<Lambda>());
            }
            if (rhs->is<Intersect>()) 
                rhs->as<Intersect>()->getFunctions(ctx, functions);
        }
    }

    Intersect::Intersect(u32 line, u32 column, const ValueClass* vc):
        BinaryOp("&", line, column, vc) {
        setType(BASE_TYPE());
    }

    Meta Intersect::fold(Stack& ctx) {
        if (type(ctx) == ERROR) return Meta();
        if (!lhs || !rhs) return Meta(type(ctx), new MetaFunction(this));
        if (type(ctx)->is<FunctionType>())
            return Meta(type(ctx), new MetaFunction(this));
        else return Meta(type(ctx), new MetaIntersect({ 
            lhs->fold(ctx), rhs->fold(ctx) 
        }));
    }
    
    Value* Intersect::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            lhs = arg;
            if (lhs->type(ctx) == TYPE) setType(find<FunctionType>(TYPE, TYPE));
            else setType(find<FunctionType>(ANY, ANY));
        }
        else if (!rhs) {
            rhs = arg;
            map<const Type*, vector<Value*>> values;
            const Type* lt = lhs->type(ctx);
            const Type* rt = rhs->type(ctx);

            if (lhs->is<Intersect>()) {
                lhs->as<Intersect>()->populate(ctx, values);
            }
            else if (lt->is<FunctionType>()) {
                const FunctionType* ft = lt->as<FunctionType>();
                values[ft->arg()].push(lhs);
            }
            else values[nullptr].push(lhs);

            if (rhs->is<Intersect>()) {
                rhs->as<Intersect>()->populate(ctx, values);
            }
            else if (rt->is<FunctionType>()) {
                const FunctionType* ft = rt->as<FunctionType>();
                values[ft->arg()].push(rhs);
            }
            else values[nullptr].push(rhs);

            auto it = values.find(ANY);
            Value* anyCase = nullptr;
            if (it != values.end()) {
                if (it->second.size() > 1) {
                    err(PHASE_TYPE, line(), column(), "More than one generic ",
                        "case in intersection.");
                    for (Value* v : it->second) {
                        note(PHASE_TYPE, v->line(), v->column(),
                             "Case: ", v);
                    }
                }
                else anyCase = it->second[0];
                values.erase(ANY);
            }

            map<const Type*, Value*> folded;
            for (auto& entry : values) {
                const Type* etype = nullptr;
                if (!entry.second.size()) continue;
                Value* v = entry.second[0];
                if (v->type(ctx))
                    etype = v->type(ctx);
                for (u32 i = 1; i < entry.second.size(); i ++) {
                    if (entry.second[i]->type(ctx)->
                        conflictsWith(v->type(ctx))) {
                        buffer b;
                        fprint(b, "Cannot create intersection; types '");
                        v->type(ctx)->format(b);
                        fprint(b, "' and '");
                        entry.second[i]->type(ctx)->format(b);
                        fprint(b, "' overlap.");
                        err(PHASE_TYPE, entry.second[i]->line(), 
                            entry.second[i]->column(), b);
                        continue;
                    }
                    if (etype && etype->is<FunctionType>() &&
                        entry.second[i]->type(ctx)->is<FunctionType>()) {
                        const FunctionType* ftype = etype->as<FunctionType>();
                        const FunctionType* eft = 
                            entry.second[i]->type(ctx)->as<FunctionType>();
                        if (ftype->ret() != eft->ret()
                            && eft->ret() != ANY && ftype->ret() != ANY) {
                            err(PHASE_TYPE, entry.second[i]->line(),
                                entry.second[i]->column(),
                                "Cannot create intersection; types '",
                                etype, "' and '", 
                                (const Type*)eft, "' would result in ",
                                "ambiguous function.");
                            continue;
                        }

                        // otherwise, if types match or one is any, 
                        // update etype
                        if (ftype->ret() == ANY) ftype = 
                            find<FunctionType>(eft->arg(), eft->ret());
                        etype = ftype;
                    }

                    Intersect* in = new Intersect(v->line(), v->column());
                    in->lhs = v;
                    in->rhs = entry.second[i];
                    in->updateType(ctx);
                    v = in;
                }
                folded[etype] = v;
            }

            Value* v = nullptr;
            for (auto& entry : folded) {
                if (!v) v = entry.second;
                else {
                    Intersect* in = new Intersect(v->line(), v->column());
                    in->lhs = v;
                    in->rhs = entry.second;
                    in->updateType(ctx);
                    v = in;
                }
                
                const Type* t = v->type(ctx);
                if (t->is<FunctionType>() 
                    && !t->as<FunctionType>()->total()
                    && anyCase) {
                    Intersect* in = new Intersect(v->line(), v->column());
                    in->lhs = v;
                    in->rhs = instantiate(ctx, anyCase->as<Lambda>(), t->as<FunctionType>()->arg());
                    in->updateType(ctx);
                    v = in;
                }
            }

            if (anyCase) {
                Intersect* in = new Intersect(v->line(), v->column());
                in->lhs = v;
                in->rhs = anyCase;
                in->updateType(ctx);
                v = in;
            }

            if (!v || !v->is<Intersect>()) {
                lhs = nullptr;
                rhs = nullptr;
                setType(ERROR);
                return this;
            }
            else {
                lhs = v->as<Intersect>()->lhs;
                rhs = v->as<Intersect>()->rhs;
                v->as<Intersect>()->lhs = v->as<Intersect>()->rhs = nullptr;
                delete v; 
            }

            updateType(ctx);
        }

        return this;
    }

    void Intersect::bindrec(const ustring& name, const Type* type,
                            const Meta& value) {
        if (!lhs || !rhs) return;
        if (lhs->is<Lambda>()) {
            lhs->as<Lambda>()->bindrec(name, type, value);
        }
        if (rhs->is<Lambda>()) {
            rhs->as<Lambda>()->bindrec(name, type, value);
        }
        if (lhs->is<Intersect>()) {
            lhs->as<Intersect>()->bindrec(name, type, value);
        }
        if (rhs->is<Intersect>()) {
            rhs->as<Intersect>()->bindrec(name, type, value);
        }
    }

    Constraint lambdaMatches(Lambda* fn, Stack& ctx, const Meta& value) {
        const FunctionType* ft = fn->type(ctx)->as<FunctionType>();
        if (auto match = ft->matches(value)) return match;
        return Constraint::NONE;
    }

    Lambda* Intersect::caseFor(Stack& ctx, const Meta& value) {
        auto it = _casecache.find(value);
        if (it != _casecache.end()) return it->second;

        Lambda *l = nullptr, *r = nullptr;
        Constraint left = Constraint::NONE, right = Constraint::NONE;
        if (lhs->is<Lambda>()) {
            left = lambdaMatches(lhs->as<Lambda>(), ctx, value);
            l = lhs->as<Lambda>();
        }
        if (rhs->is<Lambda>()) {
            right = lambdaMatches(rhs->as<Lambda>(), ctx, value);
            r = rhs->as<Lambda>();
        }
        if (lhs->is<Intersect>()) {
            l = lhs->as<Intersect>()->caseFor(ctx, value);
            left = l ? maxMatch(l->type(ctx)->as<FunctionType>()
                ->constraints(), value) : Constraint::NONE;
        }
        if (rhs->is<Intersect>()) {
            r = rhs->as<Intersect>()->caseFor(ctx, value);
            right = r ? maxMatch(r->type(ctx)->as<FunctionType>()
                ->constraints(), value) : Constraint::NONE;
        }

        if (!left && !right) return nullptr;
        else if (left) return _casecache[value] = l;
        else if (right) return _casecache[value] = r;
        else if (left.precedes(right) || 
            (left.type() != UNKNOWN && right.type() == UNKNOWN)) return _casecache[value] = l;
        else return _casecache[value] = r;
    }

    Location* Intersect::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        if (type(ctx)->is<FunctionType>()) {
            auto ft = type(ctx)->as<FunctionType>();
            if (!_label.size()) {
                Function* func = gen.newFunction();
                _label = func->label();
                vector<Lambda*> cases;
                getFunctions(ctx, cases);

                vector<Lambda*> constrained;
                Lambda* wildcard = nullptr;

                for (Lambda* l : cases) { 
                    auto& cons = l->type(ctx)->
                        as<FunctionType>()->constraints();
                    bool equals = false;
                    for (auto& con : cons) {
                        if (con.type() == EQUALS_VALUE) {
                            constrained.push(l);
                            equals = true;
                            break;
                        }
                    }
                    if (!equals) wildcard = l;
                }

                Location* arg = func->stack(ft->arg());
                func->add(new MovInsn(arg, gen.locateArg(ft->arg())));
                Location* retval = ft->ret() == VOID ? frame.none()
                    : func->stack(ft->ret());

                ustring end = gen.newLabel();
                vector<ustring> labels;
                vector<Location*> callables;
                for (u32 i = 0; i < constrained.size(); i ++) {
                    labels.push(gen.newLabel());
                }

                for (u32 i = 0; i < constrained.size(); i ++) {
                    // callables.push(constrained[i]->gen(ctx, gen, *func));
                    auto& cons = constrained[i]->type(ctx)->
                        as<FunctionType>()->constraints();
                    // func->add(new IfEqualInsn(
                    //     cons[0].value().gen(gen, *func),
                    //     arg,
                    //     labels[i]
                    // ));
                }

                Location* call = arg;
                if (wildcard) {
                    call = wildcard->genInline(ctx, arg, gen, *func);
                }
                if (*retval) func->add(new MovInsn(retval, call));
                func->add(new GotoInsn(end));

                for (u32 i = 0; i < constrained.size(); i ++) {
                    func->add(new Label(labels[i], false));

                    Location* call = constrained[i]->genInline(ctx, arg, gen, *func);
                    if (*retval) func->add(new MovInsn(retval, call));
                    func->add(new GotoInsn(end));
                }

                func->add(new Label(end, false));
                if (*retval) func->add(new RetInsn(retval));
            }
            Location* loc = frame.stack(type(ctx));
            frame.add(new LeaInsn(loc, _label));
            return loc;
        }
        return frame.none();
    }

    Value* Intersect::clone(Stack& ctx) const {
        Intersect* node = new Intersect(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    // If

    const ValueClass If::CLASS(Builtin::CLASS);

    If::If(u32 line, u32 column, const ValueClass* vc):
        Builtin(line, column, vc), cond(nullptr), body(nullptr) {
        setType(find<FunctionType>(BOOL, find<FunctionType>(ANY, VOID, true)));
    }

    If::~If() {
        if (cond) delete cond;
        if (body) delete body;
    }

    Meta If::fold(Stack& ctx) {
        if (!cond || !body) return Meta(type(ctx), new MetaFunction(this));
        Meta c = cond->fold(ctx);
        if (c.asBool()) body->fold(ctx);
        return Meta(VOID);
    }

    Value* If::apply(Stack& ctx, Value* arg) {
        if (!cond) {
            cond = arg;
            setType(find<FunctionType>(ANY, VOID, true));
        }
        else if (!body) {
            Stack* temp = new Stack(&ctx);
            arg->as<Quote>()->term()->eval(*temp);
            vector<Value*> vals;
            for (Value* v : *temp) vals.push(v);
            body = new Sequence(vals, arg->line(), arg->column());
            setType(VOID);
        }
        return this;
    }

    void If::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "If");
        if (cond) cond->format(io, level + 1);
        if (body) body->format(io, level + 1);
    }

    Value* If::clone(Stack& ctx) const {
        If* i = new If(line(), column());
        if (cond) i->apply(ctx, cond->clone(ctx));
        if (body) i->apply(ctx, body->clone(ctx));
        return i;
    }

    void If::repr(stream& io) const {
        if (!cond) print(io, "(if ??: ??)");
        else if (!body) print(io, "(if ", cond, ": ??)");
        else print(io, "(if ", cond, ": ", body, ")");
    }

    // While

    const ValueClass While::CLASS(Builtin::CLASS);

    While::While(u32 line, u32 column, const ValueClass* vc):
        Builtin(line, column, vc), cond(nullptr), body(nullptr) {
        setType(find<FunctionType>(BOOL, find<FunctionType>(ANY, VOID, true)));
    }

    While::~While() {
        if (cond) delete cond;
        if (body) delete body;
    }

    Meta While::fold(Stack& ctx) {
        if (!cond || !body) return Meta(type(ctx), new MetaFunction(this));
        Meta c = cond->fold(ctx);
        while (c.asBool()) body->fold(ctx), c = cond->fold(ctx);
        return Meta(VOID);
    }

    Value* While::apply(Stack& ctx, Value* arg) {
        if (!cond) {
            cond = arg;
            setType(find<FunctionType>(ANY, VOID, true));
        }
        else if (!body) {
            Stack* temp = new Stack(&ctx);
            arg->as<Quote>()->term()->eval(*temp);
            vector<Value*> vals;
            for (Value* v : *temp) vals.push(v);
            body = new Sequence(vals, arg->line(), arg->column());
            setType(VOID);
        }
        return this;
    }

    void While::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "While");
        if (cond) cond->format(io, level + 1);
        if (body) body->format(io, level + 1);
    }

    Value* While::clone(Stack& ctx) const {
        While* i = new While(line(), column());
        if (cond) i->apply(ctx, cond->clone(ctx));
        if (body) i->apply(ctx, body->clone(ctx));
        return i;
    }

    void While::repr(stream& io) const {
        if (!cond) print(io, "(while ??: ??)");
        else if (!body) print(io, "(while ", cond, ": ??)");
        else print(io, "(while ", cond, ": ", body, ")");
    }

    Location* While::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        ustring start = gen.newLabel(), end = gen.newLabel();
        frame.add(new Label(start, false));
        frame.add(new IfEqualInsn(
            cond->gen(ctx, gen, frame), 
            frame.add(new BoolData(false))->value(gen, frame),
            end
        ));
        body->gen(ctx, gen, frame);
        frame.add(new GotoInsn(start));
        frame.add(new Label(end, false));
    }

    // Define

    const ValueClass Define::CLASS(Builtin::CLASS);

    Define::Define(Value* type, const ustring& name,
                   const ValueClass* vc):
        Builtin(type->line(), type->column(), vc), _type(type), _name(name) {
        //
    }

    Define::~Define() {
        if (_type) delete _type;
    }

    const ustring& Define::name() const {
        return _name;
    }

    void Define::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Define ", _name);
        _type->format(io, level + 1);
    }

    Value* Define::apply(Stack& ctx, Value* arg) {
        Meta fr = _type->fold(ctx);
        if (!fr.isType()) {
            err(PHASE_TYPE, line(), column(),
                "Expected type expression, got '", fr, "'.");
            setType(ERROR);
        }
        else if (ctx.nearestScope().find(_name) != ctx.nearestScope().end()) {
            err(PHASE_TYPE, line(), column(),
                "Redefinition of variable '", _name, "'.");
            setType(ERROR);
        }
        else {
            setType(fr.asType());
            ctx.bind(_name, type(ctx));
        }

        return this;
    }

    bool Define::lvalue(Stack& ctx) {
        return true;
    }

    Stack::Entry* Define::entry(Stack& ctx) const {
        return ctx[_name];
    }
    
    bool Define::canApply(Stack& ctx, Value* arg) const {
        return !_name.size() || !_type;
    }

    Location* Define::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        if (!entry(ctx)->loc()) entry(ctx)->loc() = frame.stack(type(ctx), _name);
        return entry(ctx)->loc();
    }

    Value* Define::clone(Stack& ctx) const {
        Define* node = new Define(_type, _name);
        node->apply(ctx, nullptr);
        return node;
    }

    void Define::repr(stream& io) const {
        print(io, "(", _type, " ", _name, ")");
    }
    
    void Define::explore(Explorer& e) {
        e.visit(this);
        if (_type) _type->explore(e);
    }

    Meta Define::fold(Stack& ctx) {
        if (!_type || !_name.size()) return Meta();
        if (!entry(ctx)) return Meta();
        return entry(ctx)->value();
    }

    // Autodefine

    const ValueClass Autodefine::CLASS(Builtin::CLASS);

    Autodefine::Autodefine(u32 line, u32 column,
                           const ValueClass* vc):
        Builtin(line, column, vc), _name(nullptr), _init(nullptr) {
        setType(find<FunctionType>(ANY, ANY));
    }

    Autodefine::~Autodefine() {
        if (_name) delete _name;
        if (_init) delete _init;
    }

    void Autodefine::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Define");
        if (_name) _name->format(io, level + 1);
        if (_init) _init->format(io, level + 1);
    }

    void bind(Stack& ctx, Value* dst, Value* src) {
        if (dst->is<Variable>()) {
            const ustring& name = dst->as<Variable>()->name();
            if (ctx.nearestScope().find(name) != ctx.nearestScope().end()) {
                err(PHASE_TYPE, dst->line(), dst->column(),
                    "Redefinition of variable '", name, "'.");
                return;
            }
            ctx.bind(name, src->type(ctx));
            if (auto val = src->entry(ctx)) {
                if (val->builtin()) ctx[name]->builtin() = val->builtin();
                if (val->value()) ctx[name]->value() = val->value();
            }
            else if (src->fold(ctx)) {
                ctx[name]->value() = src->fold(ctx);
            }
            if (src->is<Lambda>()) {
                src->as<Lambda>()->bindrec(name, src->type(ctx),
                    src->fold(ctx));
            }
            if (src->is<Intersect>()) {
                src->as<Intersect>()->bindrec(name, src->type(ctx),
                    src->fold(ctx));
            }
        }
        else if (dst->is<Join>()) {
            if (!src->is<Join>()) {
                err(PHASE_TYPE, src->line(), src->column(),
                    "Attempted to bind multiple variables to ",
                    "non-tuple value.");
            }
            bind(ctx, dst->as<Join>()->left(), src->as<Join>()->left());
            bind(ctx, dst->as<Join>()->right(), src->as<Join>()->right());
        }
    }

    Value* Autodefine::apply(Stack& ctx, Value* arg) {
        if (!_name) {
            if (!arg->is<Quote>()) {
                err(PHASE_TYPE, arg->line(), arg->column(),
                    "Expected symbol.");
                return this;
            }
            catchErrors();
            u32 prev = ctx.size();
            arg->as<Quote>()->term()->eval(ctx);
            discardErrors();
            if (ctx.size() == prev + 1 && ctx.top()->is<Variable>()) {
                _name = ctx.pop();
            }
            else if (ctx.size() == prev + 1 && ctx.top()->is<Join>()) {
                _name = ctx.pop();
            }
            else {
                err(PHASE_TYPE, arg->line(), arg->column(),
                    "Expected symbol.");
            }
            setType(find<FunctionType>(ANY, VOID));
        }
        else if (!_init) {
            _init = arg;
            bind(ctx, _name, _init);
            setType(VOID);
        }
        return this;
    }
    
    bool Autodefine::canApply(Stack& ctx, Value* arg) const {
        return !_name || !_init;
    }

    bool Autodefine::lvalue(Stack& ctx) {
        return true;
    }

    Stack::Entry* Autodefine::entry(Stack& ctx) const {
        return nullptr;
    }

    Location* Autodefine::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        auto e = _name->entry(ctx);
        if (!e->reassigned() && _init->type(ctx)->is<FunctionType>()
            && _name->is<Variable>()) {
            Meta fr = _init->fold(ctx);
            if (fr.isFunction() && fr.asFunction().value()->is<Lambda>()) {
                fr.asFunction().value()->as<Lambda>()->
                    addAltLabel(_name->as<Variable>()->name());
            }
        }
        if (!e->loc()) {
            if (_name->is<Variable>())
                e->loc() = frame.stack(_init->type(ctx), _name->as<Variable>()->name());
            else e->loc() = frame.stack(_init->type(ctx));
        }
        frame.add(new MovInsn(e->loc(), _init->gen(ctx, gen, frame)));
        return e->loc();
    }

    Value* Autodefine::clone(Stack& ctx) const {
        Autodefine* node = new Autodefine(line(), column());
        if (_name) node->apply(ctx, _name);
        if (_init) node->apply(ctx, _init);
        return node;
    }

    void Autodefine::repr(stream& io) const {
        if (!_name && !_init) print(io, "let");
        else if (!_init) print(io, "(let ", _name, ")");
        else print(io, "(let ", _name, " = ", _init, ")");
    }
    
    void Autodefine::explore(Explorer& e) {
        e.visit(this);
        if (_name) _name->explore(e);
        if (_init) _init->explore(e);
    }

    Meta Autodefine::fold(Stack& ctx) {
        if (!_name || !_init) return Meta();
        _name->fold(ctx);
        if (Meta m = _init->fold(ctx))
            _name->entry(ctx)->value() = m;
        // println(_stdout, _name, " := ", _init->fold(ctx));
        return Meta(VOID);
    }

    // Assign

    const ValueClass Assign::CLASS(Builtin::CLASS);

    Assign::Assign(u32 line, u32 column, const ValueClass* vc):
        Builtin(line, column, vc), lhs(nullptr), rhs(nullptr) {
        setType(find<FunctionType>(ANY, find<FunctionType>(ANY, ANY), true));
    }

    Assign::~Assign() {
        if (lhs) delete lhs;
        if (rhs) delete rhs;
    }

    void assign(Stack& ctx, Value* dst, Value* src) {
        if (dst->type(ctx)->is<ReferenceType>()) {
            basil::assign(dst->fold(ctx).asRef(), src->fold(ctx));
        }
        else if (dst->is<Join>()) {
            if (!src->is<Join>()) {
                err(PHASE_TYPE, src->line(), src->column(),
                    "Attempted to assign multiple variables to ",
                    "non-tuple value.");
            }
            assign(ctx, dst->as<Join>()->left(), src->as<Join>()->left());
            assign(ctx, dst->as<Join>()->right(), src->as<Join>()->right());
        }
        else if (dst->is<Variable>() || dst->is<Define>()) {
            if (auto val = src->entry(ctx)) {
                auto entry = dst->entry(ctx);
                if (dst->is<Variable>()) entry->reassign();
                if (val->builtin()) entry->builtin() = val->builtin();
                if (val->value()) entry->value() = val->value();
            }
            else if (src->fold(ctx)) {
                auto entry = dst->entry(ctx);
                if (dst->is<Variable>()) entry->reassign();
                entry->value() = src->fold(ctx);
            }
        }
    }
    
    Value* Assign::apply(Stack& ctx, Value* arg) {
        if (!lhs) {
            Stack* temp = new Stack(&ctx);
            arg->as<Quote>()->term()->eval(*temp);
            if (temp->size() > 1) {
                err(PHASE_TYPE, line(), column(),
                    "More than one destination provided to assignment.");
                return nullptr;
            }
            else if (temp->size() == 0) {
                err(PHASE_TYPE, line(), column(),
                    "No destination provided to assignment.");
                return nullptr;
            }
            else {
                arg = temp->top();
                temp->clear();
            }
            if (!arg->lvalue(ctx)) {
                err(PHASE_TYPE, line(), column(),
                    "Value on left side of assignment is not assignable.");
            }
            lhs = arg;
            if (lhs->is<Autodefine>())
                setType(find<FunctionType>(ANY, ANY));
            else if (lhs->is<Variable>() && !lhs->entry(ctx))
                setType(find<FunctionType>(ANY, ANY));
            else if (lhs->type(ctx)->is<ReferenceType>())
                setType(find<FunctionType>(lhs->type(ctx)->as<ReferenceType>()->element(), ANY));
            else 
                setType(find<FunctionType>(lhs->type(ctx), ANY));
        }
        else if (!rhs) {
            rhs = arg;
            if (lhs->is<Autodefine>()) {
                lhs->as<Autodefine>()->apply(ctx, rhs);
                Value* ret = lhs;
                lhs = rhs = nullptr;
                delete this;
                return ret;
            }
            if (lhs->is<Variable>() && !lhs->entry(ctx)) {
                Quote* name = new Quote(
                    new VariableTerm(lhs->as<Variable>()->name(), lhs->line(), lhs->column()),
                    lhs->line(), lhs->column()
                );
                Autodefine* def = new Autodefine(line(), column());
                def->apply(ctx, name);
                def->apply(ctx, rhs);
                rhs = nullptr;
                delete this;
                return def;
            }

            const Type* dstt = lhs->type(ctx);
            if (dstt->is<ReferenceType>()) 
                dstt = dstt->as<ReferenceType>()->element();
            if (rhs->type(ctx) != dstt) {
                rhs = new Cast(dstt, rhs);
            }
            setType(VOID);
        }
        return this;
    }

    void Assign::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Assign");
        if (lhs) lhs->format(io, level + 1);
        if (rhs) rhs->format(io, level + 1);
    }

    bool Assign::lvalue(Stack& ctx) {
        return true;
    }

    Stack::Entry* Assign::entry(Stack& ctx) const {
        return lhs->entry(ctx);
    }

    Location* Assign::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        if (shouldAlloca(lhs->type(ctx))) {
            Location* r = rhs->gen(ctx, gen, frame);
            Location* size = frame.add(new SizeofInsn(r))->value(gen, frame);
            Location* mem = frame.add(new AllocaInsn(size, lhs->type(ctx)))->value(gen, frame);
            frame.add(new MemcpyInsn(mem, r, size, gen.newLabel()));
            frame.add(new MovInsn(lhs->gen(ctx, gen, frame), mem));
        }
        else frame.add(new MovInsn(lhs->gen(ctx, gen, frame), rhs->gen(ctx, gen, frame)));
        return frame.none();
    }

    Value* Assign::clone(Stack& ctx) const {
        Assign* node = new Assign(line(), column());
        if (lhs) node->apply(ctx, lhs);
        if (rhs) node->apply(ctx, rhs);
        return node;
    }

    void Assign::repr(stream& io) const {
        if (!lhs && !rhs) print(io, "=");
        else if (!rhs) print(io, "(", lhs, " =)");
        else print(io, "(", lhs, " = ", rhs, ")");
    }
    
    void Assign::explore(Explorer& e) {
        e.visit(this);
        if (lhs) lhs->explore(e);
        if (rhs) rhs->explore(e);
    }

    Meta Assign::fold(Stack& ctx) {
        if (!lhs || !rhs) return Meta();
        Meta l = lhs->fold(ctx);
        if (l.isRef()) l.asRef() = rhs->fold(ctx);
        else lhs->entry(ctx)->value() = rhs->fold(ctx);
        return Meta(VOID);
    }

    // Print

    const Type* Print::_BASE_TYPE;

    const Type* Print::BASE_TYPE() {
        if (_BASE_TYPE) return _BASE_TYPE;
        return _BASE_TYPE = find<IntersectionType>(set<const Type*>({
            find<FunctionType>(STRING, VOID),
            find<FunctionType>(CHAR, VOID),
            find<FunctionType>(BOOL, VOID),
            find<FunctionType>(I64, VOID),
            find<FunctionType>(DOUBLE, VOID)
        }));
    }

    const ValueClass Print::CLASS(UnaryOp::CLASS);

    Print::Print(u32 line, u32 column, const ValueClass* vc):
        UnaryOp("print", line, column, vc) {
        setType(BASE_TYPE());
    }

    Value* Print::apply(Stack& ctx, Value* arg) {
        if (!_operand) {
            _operand = arg;
            setType(VOID);
        }
        return this;
    }
    
    Location* Print::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        if (_operand->type(ctx) == I64)
            frame.add(new CCallInsn({_operand->gen(ctx, gen, frame)}, "_printi64", VOID))->value(gen, frame);
        else if (_operand->type(ctx) == DOUBLE)
            frame.add(new CCallInsn({_operand->gen(ctx, gen, frame)}, "_printf64", VOID))->value(gen, frame);
        else if (_operand->type(ctx) == STRING)
            frame.add(new CCallInsn({_operand->gen(ctx, gen, frame)}, "_printstr", VOID))->value(gen, frame);
        return frame.none();
    }

    Value* Print::clone(Stack& ctx) const {
        Print* node = new Print(line(), column());
        if (_operand) node->apply(ctx, _operand);
        return node;
    }

    Meta Print::fold(Stack& ctx) {
        Meta m = _operand->fold(ctx);
        if (!m) {
            err(PHASE_TYPE, _operand->line(), _operand->column(),
                "Could not evaluate value for compile-time print.");
        }
        else println(_stdout, m);
        return Meta(VOID);
    }

    // Typeof

    const ValueClass Typeof::CLASS(UnaryOp::CLASS);

    Typeof::Typeof(u32 line, u32 column, const ValueClass* vc):
        UnaryOp("Typeof", line, column, vc) {
        setType(find<FunctionType>(ANY, TYPE));
    }

    Value* Typeof::apply(Stack& ctx, Value* arg) {
        setType(TYPE);
        return UnaryOp::apply(ctx, arg);
    }

    Value* Typeof::clone(Stack& ctx) const {
        Typeof* t = new Typeof(line(), column());
        if (_operand) t->apply(ctx, _operand);
        return t;
    }

    Meta Typeof::fold(Stack& ctx) {
        if (!_operand) return Meta();
        return Meta(TYPE, _operand->type(ctx));
    }

    // Cast

    const ValueClass Cast::CLASS(Value::CLASS);

    Cast::Cast(const Type* dst, Value* src,
                               const ValueClass* vc):
        Value(src->line(), src->column(), vc), _dst(dst), _src(src) {
        setType(dst);
    }
                    
    void Cast::format(stream& io, u32 level) const {
        indent(io, level);
        _dst->format(io);
        println(io, " cast");
        _src->format(io, level + 1);
    }
    
    Meta Cast::fold(Stack& ctx) {
        // TODO: casting rules
        if (_dst == TYPE) {
            const Type* t = _src->type(ctx);
            if (t->is<FunctionType>()) {
                if (!_src->fold(ctx).isFunction()) {
                    err(PHASE_TYPE, line(), column(),
                        "Cannot find function.");
                    return Meta();
                }
                auto& cons = t->as<FunctionType>()->constraints();
                if (cons.size() != 1 || cons[0].type() != EQUALS_VALUE
                    || !cons[0].value().isType()) {
                    err(PHASE_TYPE, line(), column(),
                        "Cannot convert function to type object.");
                    return Meta();
                }
                else {    
                    Value* fn = _src->fold(ctx).asFunction().value();
                    const Type* bt = nullptr;
                    if (fn->is<Lambda>()) {
                        bt = fn->as<Lambda>()->body()->fold(ctx).asType();
                    }
                    return Meta(TYPE, find<FunctionType>(cons[0].value().asType(), bt));
                }
            }
            else if (t->is<TupleType>()) {
                vector<const Type*> ts;
                Meta m = _src->fold(ctx);
                if (!m.isTuple()) {
                    err(PHASE_TYPE, line(), column(),
                        "Cannot evaluate tuple at compile-time.");
                    return Meta();
                }
                for (Meta& e : m.asTuple()) {
                    ts.push(e.asType());
                }
                return Meta(TYPE, find<TupleType>(ts));
            }
            return _src->fold(ctx);
        }
        else if (_src->type(ctx)->is<ReferenceType>()) {
            Meta m = _src->fold(ctx);
            return m.asRef();
        }
        else if (_dst->is<NumericType>()) {
            if (_dst->as<NumericType>()->floating()) {
                if (!_src->fold(ctx)) return Meta();
                return Meta(_dst, toFloat(_src->fold(ctx)));
            }
            else {
                if (!_src->fold(ctx)) return Meta();
                return Meta(_dst, trunc(toInt(_src->fold(ctx)), _dst));
            }
        }
        return Meta();
    }
    
    Location* Cast::gen(Stack& ctx, CodeGenerator& gen, CodeFrame& frame) {
        return frame.add(new CastInsn(_src->gen(ctx, gen, frame), _dst))->value(gen, frame);
    }

    Value* Cast::clone(Stack& ctx) const {
        return new Cast(_dst, _src);
    }

    void Cast::repr(stream& io) const {
        print(io, "(", _src, " as ", _dst, ")");
    }
    
    void Cast::explore(Explorer& e) {
        e.visit(this);
        if (_src) _src->explore(e);
    }
    
    bool Cast::lvalue(Stack& ctx) {
        return _src && _src->type(ctx)->is<ReferenceType>() 
            && !_dst->is<ReferenceType>();
    }

    // Eval

    const ValueClass Eval::CLASS(Builtin::CLASS);

    Eval::Eval(u32 line, u32 column, const ValueClass* vc):
        Builtin(line, column, vc) {
        setType(find<FunctionType>(ANY, ANY));
    }

    void Eval::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "eval");
    }

    Value* eval(Stack& ctx, const Meta& m, u32 line, u32 column) {
        if (m.isArray()) {
            Stack* tmp = new Stack(&ctx);
            for (const Meta& i : m.asArray())
                tmp->push(eval(*tmp, i, line, column));
            for (Value* v : *tmp) ctx.push(v);
            tmp->clear();
            return nullptr;
        }
        else if (m.isUnion())
            return eval(ctx, m.asUnion().value(), line, column);
        else if (m.isSymbol())
            return new Variable(
                findSymbol(m.asSymbol()), line, column
            );
        else if (m.isInt()) 
            return new IntegerConstant(m.asInt(), line, column);
        else if (m.isFloat())
            return new RationalConstant(m.asFloat(), line, column);
        else if (m.isBool())
            return new BoolConstant(m.asBool(), line, column);
        else if (m.isString())
            return new StringConstant(m.asString(), line, column);
        else if (m.isVoid())
            return new Void(line, column);
        else err(PHASE_TYPE, line, column,
                 "Could not evaluate term.");
        return nullptr;
    }

    Value* Eval::apply(Stack& ctx, Value* v) {
        if (!v) return this;
        
        // Stack* s = &ctx;
        // while (s) {
        //     if (s->hasScope()) for (auto &e : s->scope()) {
        //         println(_stdout, e.first, ": ", e.second.type(), "|", e.second.value().toString());
        //     }
        //     println("");
        //     println("");
        //     s = s->parent();
        // }

        Value* ret = eval(ctx, v->fold(ctx), v->line(), v->column());
        delete this;
        return ret;
    }

    Value* Eval::clone(Stack& ctx) const {
        return new Eval(line(), column());
    }

    void Eval::repr(stream& io) const {
        print(io, "eval");
    }

    // MetaEval

    const ValueClass MetaEval::CLASS(Builtin::CLASS);

    MetaEval::MetaEval(u32 line, u32 column, 
            const ValueClass* vc):
        Builtin(line, column, vc) {
        setType(find<FunctionType>(ANY, ANY));
    }

    void MetaEval::format(stream& io, u32 level) const {
        indent(io, level);
        println("meta");
    }

    Value* MetaEval::apply(Stack& ctx, Value* v) {
        val = v->fold(ctx);
        return this;
    }

    Value* MetaEval::clone(Stack& ctx) const {
        return new MetaEval(line(), column());
    }

    void MetaEval::repr(stream& io) const {
        print(io, "meta");
    }

    Meta MetaEval::fold(Stack& ctx) {
        return val;
    }

    // Use

    const ValueClass Use::CLASS(Builtin::CLASS);

    Use::Use(u32 line, u32 column, const ValueClass* vc):
        Builtin(line, column, vc), path(nullptr), src(nullptr), 
        env(nullptr), module(nullptr) {
        setType(find<FunctionType>(STRING, VOID));
    }

    Use::~Use() {
        if (path) delete path;
        if (src) delete src;
        if (env) delete env;
        if (module) delete module;
    }

    void Use::format(stream& io, u32 level) const {
        indent(io, level);
        println("Use");
        if (path) path->format(io, level + 1);
    }

    Value* Use::apply(Stack& ctx, Value* v) {
        if (!path) {
            path = v;
            setType(VOID);
            Meta m = path->fold(ctx);
            if (!m.isString()) {
                err(PHASE_TYPE, v->line(), v->column(),
                    "Module path must be constant string.");
                return this;
            }
            const ustring& s = m.asString();
            string asciipath;
            buffer b;
            fprint(b, s);
            fread(b, asciipath);
            const char* path = (const char*)asciipath.raw();
            Module* mod = loadModule(path, v->line(), v->column());
            if (!mod) return this;
            mod->useIn(ctx, line(), column());
        }  
        return this;
    }

    Value* Use::clone(Stack& ctx) const {
        Use* u = new Use(line(), column());
        if (path) u->apply(ctx, path);
        return u;
    }

    void Use::repr(stream& io) const {
        if (!path) print(io, "(use ??)");
        else print(io, "(use ", path, ")");
    }
}

void print(stream& io, basil::Value* v) {
    v->repr(io);
}

void print(basil::Value* v) {
    print(_stdout, v);
}