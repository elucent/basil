#include "term.h"
#include "io.h"
#include "value.h"
#include "errors.h"
#include "type.h"

namespace basil {

    // TermClass
    
    TermClass::TermClass(): _parent(nullptr) {
        //
    }

    TermClass::TermClass(const TermClass& parent): _parent(&parent) {
        //
    }

    const TermClass* TermClass::parent() const {
        return _parent;
    }

    // Term

    const TermClass Term::CLASS;
    
    void Term::indent(stream& io, u32 level) const {
        while (level) -- level, print(io, "    ");
    }
    
    void Term::setParent(Term* parent) {
        _parent = parent;
    }

    Term::Term(u32 line, u32 column, const TermClass* tc):
        _line(line), _column(column), _parent(nullptr), _termclass(tc) {
        //
    }

    Term::~Term() {
        for (Term* t : _children) delete t;
    }

    u32 Term::line() const {
        return _line;
    }

    u32 Term::column() const {
        return _column;
    }

    // IntegerTerm

    const TermClass IntegerTerm::CLASS(Term::CLASS);

    IntegerTerm::IntegerTerm(i64 value, u32 line, u32 column, 
                                const TermClass* tc):
        Term(line, column, tc), _value(value) {
        //
    }

    i64 IntegerTerm::value() const {
        return _value;
    }

    void IntegerTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Integer ", _value);
    }

    void IntegerTerm::eval(Stack& stack) {
        stack.push(new IntegerConstant(_value, line(), column()));
    }
    
    bool IntegerTerm::equals(const Term* other) const {
        return other->is<IntegerTerm>() 
            && _value == other->as<IntegerTerm>()->_value;
    }

    u64 IntegerTerm::hash() const {
        return ::hash(_value);
    }

    Term* IntegerTerm::clone() const {
        return new IntegerTerm(_value, line(), column());
    }

    // RationalTerm

    const TermClass RationalTerm::CLASS(Term::CLASS);

    RationalTerm::RationalTerm(double value, u32 line, u32 column,
                                const TermClass* tc):
        Term(line, column, tc), _value(value) {
        //
    }

    double RationalTerm::value() const {
        return _value;
    }

    void RationalTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Rational ", _value);
    }

    void RationalTerm::eval(Stack& stack) {
        stack.push(new RationalConstant(_value, line(), column()));
    }
    
    bool RationalTerm::equals(const Term* other) const {
        return other->is<RationalTerm>() 
            && _value == other->as<RationalTerm>()->_value;
    }

    u64 RationalTerm::hash() const {
        return ::hash(_value);
    }

    Term* RationalTerm::clone() const {
        return new RationalTerm(_value, line(), column());
    }

    // StringTerm

    const TermClass StringTerm::CLASS(Term::CLASS);

    StringTerm::StringTerm(const ustring& value, u32 line, u32 column,
                            const TermClass* tc):
        Term(line, column, tc), _value(value) {
        //
    }

    const ustring& StringTerm::value() const {
        return _value;
    }

    void StringTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "String \"", _value, "\"");
    }

    void StringTerm::eval(Stack& stack) {
        stack.push(new StringConstant(_value, line(), column()));
    }
    
    bool StringTerm::equals(const Term* other) const {
        return other->is<StringTerm>() 
            && _value == other->as<StringTerm>()->_value;
    }

    u64 StringTerm::hash() const {
        return ::hash(_value);
    }

    Term* StringTerm::clone() const {
        return new StringTerm(_value, line(), column());
    }

    // CharTerm

    const TermClass CharTerm::CLASS(Term::CLASS);

    CharTerm::CharTerm(uchar value, u32 line, u32 column,
                        const TermClass* tc):
        Term(line, column, tc), _value(value) {
        //
    }

    uchar CharTerm::value() const {
        return _value;
    }

    void CharTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Char '", _value, "'");
    }

    void CharTerm::eval(Stack& stack) {
        stack.push(new CharConstant(_value, line(), column()));
    }
    
    bool CharTerm::equals(const Term* other) const {
        return other->is<CharTerm>() 
            && _value == other->as<CharTerm>()->_value;
    }

    u64 CharTerm::hash() const {
        return ::hash(_value);
    }

    Term* CharTerm::clone() const {
        return new CharTerm(_value, line(), column());
    }

    // BoolTerm
        
    const TermClass BoolTerm::CLASS(Term::CLASS);

    BoolTerm::BoolTerm(bool value, u32 line, u32 column,
                       const TermClass* tc):
        Term(line, column, tc), _value(value) {
        //
    }

    bool BoolTerm::value() const {
        return _value;
    }

    void BoolTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Boolean ", _value);
    }

    void BoolTerm::eval(Stack& stack) {
        stack.push(new BoolConstant(_value, line(), column()));
    }

    bool BoolTerm::equals(const Term* other) const {
        return other->is<BoolTerm>() && 
            _value == other->as<BoolTerm>()->_value;
    }

    u64 BoolTerm::hash() const {
        return ::hash(_value);
    }

    Term* BoolTerm::clone() const {
        return new BoolTerm(_value, line(), column());
    }

    // VoidTerm

    const TermClass VoidTerm::CLASS(Term::CLASS);

    VoidTerm::VoidTerm(u32 line, u32 column, const TermClass* tc):
        Term(line, column, tc) {
        //
    }

    void VoidTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Void ()");
    }

    void VoidTerm::eval(Stack& stack) {
        stack.push(new Void(line(), column()));
    }

    bool VoidTerm::equals(const Term* other) const {
        return other->is<VoidTerm>();
    }

    u64 VoidTerm::hash() const {
        return 14517325296099750659ul;
    }

    Term* VoidTerm::clone() const {
        return new VoidTerm(line(), column());
    }
    
    // EmptyTerm

    const TermClass EmptyTerm::CLASS(Term::CLASS);

    EmptyTerm::EmptyTerm(u32 line, u32 column, const TermClass* tc):
        Term(line, column, tc) {
        //
    }

    void EmptyTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Empty []");
    }

    void EmptyTerm::eval(Stack& stack) {
        stack.push(new Empty(line(), column()));
    }

    bool EmptyTerm::equals(const Term* other) const {
        return other->is<EmptyTerm>();
    }

    u64 EmptyTerm::hash() const {
        return 429888988482187327ul;
    }

    Term* EmptyTerm::clone() const {
        return new EmptyTerm(line(), column());
    }

    // VariableTerm

    const TermClass VariableTerm::CLASS(Term::CLASS);

    VariableTerm::VariableTerm(const ustring& name, u32 line, u32 column,
                                const TermClass* tc):
        Term(line, column, tc), _name(name) {
        //
    }

    const ustring& VariableTerm::name() const {
        return _name;
    }

    void VariableTerm::rename(const ustring& name) {
        _name = name;
    }

    void VariableTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Variable ", _name);
    }

    void VariableTerm::eval(Stack& stack) {
        auto entry = stack[_name];
        if (entry && entry->meta()) stack.push(entry->meta()->clone(stack));
        else stack.push(new Variable(_name, line(), column()));
    }
    
    bool VariableTerm::equals(const Term* other) const {
        return other->is<VariableTerm>() 
            && _name == other->as<VariableTerm>()->_name;
    }

    u64 VariableTerm::hash() const {
        return ::hash(_name) ^ ::hash("var");
    }

    Term* VariableTerm::clone() const {
        return new VariableTerm(_name, line(), column());
    }

    // BlockTerm

    const TermClass BlockTerm::CLASS(Term::CLASS);

    BlockTerm::BlockTerm(const vector<Term*>& children, u32 line, u32 column,
                            const TermClass* tc):
        Term(line, column, tc) {
        _children = children;
        for (Term* t : _children) t->setParent(this);
    }

    const vector<Term*>& BlockTerm::children() const {
        return _children;
    }

    void BlockTerm::add(Term* child) {
        _children.push(child);
    }

    void BlockTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println("Block");
        for (const Term* t : _children) t->format(io, level + 1);
    }

    void BlockTerm::eval(Stack& stack) {
        Stack* local = new Stack(&stack);
        for (Term* t : _children) {
            if (local->expectsMeta()) {
                local->push(new Quote(t, t->line(), t->column()));
            }
            else t->eval(*local);
        }
        for (Value* v : *local) stack.push(v);
    }
    
    bool BlockTerm::equals(const Term* other) const {
        if (!other->is<BlockTerm>()) return false;
        if (_children.size() != other->as<BlockTerm>()->_children.size()) 
            return false;
        for (u32 i = 0; i < _children.size(); i ++) {
            if (!_children[i]->equals(other->as<BlockTerm>()->_children[i]))
                return false;
        }
        return true;
    }

    u64 BlockTerm::hash() const {
        u64 h = 0;
        for (Term* t : _children) h ^= t->hash();
        return h;
    }

    Term* BlockTerm::clone() const {
        vector<Term*> children;
        for (Term* child : _children) children.push(child->clone());
        return new BlockTerm(children, line(), column());
    }

    // ProgramTerm

    const TermClass ProgramTerm::CLASS(Term::CLASS);
    
    template<typename T>
    static Value* factory(const Value* t) {
        return new T(t->line(), t->column());
    }

    static Value* macro(const Value* t) {
        return new Macro(false, t->line(), t->column());
    }

    static Value* metamacro(const Value* t) {
        return new Macro(true, t->line(), t->column());
    }
    
    void ProgramTerm::initRoot() {
        root->bind("+", BinaryMath::BASE_TYPE(), factory<Add>);
        root->bind("-", BinaryMath::BASE_TYPE(), factory<Subtract>);
        root->bind("*", BinaryMath::BASE_TYPE(), factory<Multiply>);
        root->bind("/", BinaryMath::BASE_TYPE(), factory<Divide>);
        root->bind("%", BinaryMath::BASE_TYPE(), factory<Modulus>);
        root->bind(",", Join::BASE_TYPE(), factory<Join>);
        root->bind("&", Intersect::BASE_TYPE(), factory<Intersect>);
        root->bind("and", BinaryLogic::BASE_TYPE(), factory<And>);
        root->bind("or", BinaryLogic::BASE_TYPE(), factory<Or>);
        root->bind("xor", BinaryLogic::BASE_TYPE(), factory<Xor>);
        root->bind("not", BinaryLogic::BASE_TYPE(), factory<Not>);
        root->bind("==", BinaryEquality::BASE_TYPE(), factory<Equal>);
        root->bind("!=", BinaryEquality::BASE_TYPE(), factory<Inequal>);
        root->bind("<", BinaryRelation::BASE_TYPE(), factory<Less>);
        root->bind("<=", BinaryRelation::BASE_TYPE(), factory<LessEqual>);
        root->bind(">", BinaryRelation::BASE_TYPE(), factory<Greater>);
        root->bind(">=", BinaryRelation::BASE_TYPE(), factory<GreaterEqual>);
        root->bind("::", find<FunctionType>(ANY, 
            find<FunctionType>(ANY, ANY)), factory<Cons>);
        root->bind("print", Print::BASE_TYPE(), factory<Print>);
        root->bind("metaprint", Metaprint::BASE_TYPE(), factory<Metaprint>);
        root->bind("log", Metaprint::BASE_TYPE(), factory<Metaprint>);
        root->bind("set!", find<FunctionType>(ANY, 
            find<FunctionType>(ANY, ANY)), factory<Assign>);
        root->bind("lambda!", find<MacroType>(ANY, true), factory<Lambda>);
        root->bind("lambda", find<MacroType>(ANY, true), factory<Lambda>);
        root->bind("λ", find<MacroType>(ANY, true), factory<Lambda>);
        root->bind("macro!", find<MacroType>(ANY, true), macro);
        root->bind("macro", find<MacroType>(ANY, true), macro);
        root->bind("metamacro!", find<MacroType>(ANY, true), metamacro);
        root->bind("metamacro", find<MacroType>(ANY, true), metamacro);
        root->bind("define!", find<MacroType>(ANY, true), 
            factory<Autodefine>);
        root->bind("define", find<MacroType>(ANY, true), 
            factory<Autodefine>);
        root->bind("let", find<MacroType>(ANY, true), 
            factory<Autodefine>);
        root->bind("eval!", find<MacroType>(ANY), factory<Eval>);
        root->bind("eval", find<MacroType>(ANY), factory<Eval>);

        // root->bind("true", new BoolConstant(true, 0, 0));
        // root->bind("false", new BoolConstant(false, 0, 0));

        root->bind("i8", TYPE, Meta(TYPE, I8));
        root->bind("i16", TYPE, Meta(TYPE, I16));
        root->bind("i32", TYPE, Meta(TYPE, I32));
        root->bind("i64", TYPE, Meta(TYPE, I64));

        root->bind("u8", TYPE, Meta(TYPE, U8));
        root->bind("u16", TYPE, Meta(TYPE, U16));
        root->bind("u32", TYPE, Meta(TYPE, U32));
        root->bind("u64", TYPE, Meta(TYPE, U64));

        root->bind("f32", TYPE, Meta(TYPE, FLOAT));
        root->bind("f64", TYPE, Meta(TYPE, DOUBLE));
        
        root->bind("char", TYPE, Meta(TYPE, CHAR));
        root->bind("string", TYPE, Meta(TYPE, STRING));

        root->bind("symbol", TYPE, Meta(TYPE, SYMBOL));
        root->bind("type", TYPE, Meta(TYPE, TYPE));
        root->bind("bool", TYPE, Meta(TYPE, BOOL));
        root->bind("void", TYPE, Meta(TYPE, VOID));
    }

    ProgramTerm::ProgramTerm(const vector<Term*>& children, 
                             u32 line, u32 column, const TermClass* tc):
        Term(line, column, tc), 
        root(new Stack(nullptr, true)), global(new Stack(root, true)) {
        _children = children;
        for (Term* t : _children) t->setParent(this);
        initRoot();
    }

    ProgramTerm::~ProgramTerm() {
        delete root;
    }

    const vector<Term*>& ProgramTerm::children() const {
        return _children;
    }

    void ProgramTerm::add(Term* child) {
        _children.push(child);
    }

    void ProgramTerm::format(stream& io, u32 level) const {
        indent(io, level);
        println(io, "Program");
        for (const Term* t : _children) t->format(io, level + 1);
    } 

    void ProgramTerm::eval(Stack& stack) {
        for (Term* t : _children) {
            if (global->expectsMeta()) {
                global->push(new Quote(t, t->line(), t->column()));
            }
            else t->eval(*global);
        }
        for (Value* v : *global) v->type(*global);
        
        vector<Value*> vals;
        for (Value* v : *global) vals.push(v);
        stack.push(new Program(vals, 1, 1)); 
    }

    Stack& ProgramTerm::scope() {
        return *global;
    }

    void ProgramTerm::evalChild(Stack& stack, Term* t) {
        Stack* local = new Stack(global);
        t->eval(*local);
        for (Value* v : *local) v->type(*local);
        stack.copy(*local);
    }
    
    bool ProgramTerm::equals(const Term* other) const {
        if (!other->is<ProgramTerm>()) return false;
        if (_children.size() != other->as<ProgramTerm>()->_children.size()) 
            return false;
        for (u32 i = 0; i < _children.size(); i ++) {
            if (!_children[i]->equals(other->as<ProgramTerm>()->_children[i]))
                return false;
        }
        return true;
    }

    u64 ProgramTerm::hash() const {
        u64 h = 0;
        for (Term* t : _children) h ^= t->hash();
        return h;
    }

    Term* ProgramTerm::clone() const {
        vector<Term*> children;
        for (Term* child : _children) children.push(child->clone());
        return new ProgramTerm(children, line(), column());
    }
}