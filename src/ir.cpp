#include "ir.h"
#include "type.h"
#include "x64.h"

namespace basil {
    const char* REGISTER_NAMES[65] = {
        "rax",
        "rcx",
        "rdx",
        "rbx",
        "rsp",
        "rbp",
        "rsi",
        "rdi",
        "r8",
        "r9",
        "r10",
        "r11",
        "r12",
        "r13",
        "r14",
        "r15",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "xmm0",
        "xmm1",
        "xmm2",
        "xmm3",
        "xmm4",
        "xmm5",
        "xmm6",
        "xmm7",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "NONE"
    };

    // Location

    Location::Location():
        segm(INVALID), imm(nullptr) {
        //
    }

    Location::Location(i64 imm_in):
        segm(IMMEDIATE), off(imm_in), type(I64), imm(nullptr) {
        //
    }

    Location::Location(Register reg_in, const Type* type_in):
        segm(REGISTER), reg(reg_in), type(type_in), imm(nullptr) {
        //
    }

    Location::Location(Register reg_in, i64 offset, const Type* type_in):
        segm(REGISTER_RELATIVE), off(offset), reg(reg_in), type(type_in), imm(nullptr) {
        //
    }
    
    Location::Location(Segment segm_in, i64 offset, const Type* type_in):
        segm(segm_in), off(offset), type(type_in), imm(nullptr) {
        //
    }

    Location::Location(const Type* type_in, const ustring& name_in):
        segm(UNASSIGNED), type(type_in), imm(nullptr), name(name_in) {
        //
    }

    Location::Location(const Type* type_in, Data* imm_in, const ustring& name_in):
        segm(DATA), type(type_in), imm(imm_in), name(name_in) {
        //
    }
    
    Location::Location(const Type* type_in, Location* base, 
                       i64 offset, const ustring& name_in):
        segm(RELATIVE), off(offset), type(type_in), imm(nullptr), src(base), name(name_in) {
        //
    }

    Location::operator bool() const {
        return segm != INVALID || (segm == RELATIVE && !*src);
    }
    
    void Location::allocate(Segment segm_in, i64 offset) {
        segm = segm_in;
        off = offset;
    }

    void Location::allocate(Register reg_in) {
        segm = REGISTER;
        reg = reg_in;
    }

    void Location::kill() {
        segm = INVALID;
    }

    bool Location::operator==(const Location& other) const {
        return other.segm == segm && (
            (segm == STACK && off == other.off)
            || (segm == REGISTER && reg == other.reg)
            || (segm == REGISTER_RELATIVE && 
                off == other.off && reg == other.reg)
            || (segm == IMMEDIATE && off == other.off)
            || (segm == DATA && off == other.off)
            || (segm == UNASSIGNED)
            || (segm == RELATIVE && src->operator==(*other.src) 
                && off == other.off)
            || (segm == INVALID)
        );
    }

    // CodeFrame

    Location CodeFrame::NONE;

    Location* CodeFrame::none() const {
        return &NONE;
    }

    // Passes

    void hlCanonicalize(CodeFrame& frame, vector<Insn*>& insns) {
        vector<Insn*> newInsns;
        MovInsn* prev = nullptr;
        for (u32 i = 0; i < insns.size(); i ++) {
            if (insns[i]->is<MovInsn>()) {
                MovInsn* cur = insns[i]->as<MovInsn>();
                if (prev && *prev->dst() == *cur->src()) {
                    prev = cur;
                    prev->dst() = cur->dst();
                    continue;
                }
                prev = cur;
            }
            newInsns.push(insns[i]);
        }
        insns = newInsns;
    }

    void livenessPass(CodeFrame& frame, vector<Insn*>& insns) {
        vector<u32> todo;
        set<Location*> empty;
        if (insns.size())
            insns.back()->liveout(frame, empty);

        for (i64 i = i64(insns.size()) - 2; i >= 0; i --) {
            if (insns[i]->liveout(frame, insns[i + 1]->inset()))
                todo.push(i);
        }

        while (todo.size()) {
            u32 last = todo[0];
            todo.clear();

            if (last == i64(insns.size()) - 1) {
                if (insns[last]->liveout(frame, empty))
                    todo.push(last);
            }
            else if (insns[last]->liveout(frame, insns[last + 1]->inset()))
                todo.push(last);
            
            for (i64 i = last; i >= 0; i --) {
                if (insns[i]->liveout(frame, insns[i + 1]->inset()))
                    todo.push(i);
            }
        }

        // for (Insn* i : insns) {
        //     i->format(_stdout);
        //     print(_stdout, "        live = { ");
        //     for (Location* l : i->inset()) 
        //         if (l->name[1] == 't') print(_stdout, l->name, " ");
        //     print(_stdout, "} -> { ");
        //     for (Location* l : i->outset()) 
        //         if (l->name[1] == 't') print(_stdout, l->name, " ");
        //     println(_stdout, "}");
        // }
    }

    void allocationPass(CodeFrame& frame, vector<Insn*>& insns) {
        vector<vector<Location*>> allocations;
        vector<vector<Location*>> frees;

        static Register order[] = {
            RCX, RBX, R8, R9, R10, R11, R12, R13, R14, R15
        };
        set<Register> available = {
            RCX, RBX, R8, R9, R10, R11, R12, R13, R14, R15
        };
        static Register fporder[] = {
            XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6
        };
        set<Register> fpavailable = {
            XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6
        };

        for (u32 i = 0; i < insns.size(); i ++) {
            allocations.push({}), frees.push({});
            for (Location* l : insns[i]->outset()) {
                if (l->segm == UNASSIGNED &&
                    insns[i]->inset().find(l) == insns[i]->inset().end()) {
                    allocations[i].push(l);
                }
            }
            for (Location* l : insns[i]->inset()) {
                if (insns[i]->outset().find(l) == insns[i]->outset().end()) {
                    frees[i].push(l);
                }
            }
        }

        // for (u32 i = 0; i < allocations.size(); i ++) {
        //     print(_stdout, "starts ", i, ": { ");
        //     for (Location* l : allocations[i]) print(_stdout, l->name, " ");
        //     println(_stdout, "}");
        // }

        // for (u32 i = 0; i < frees.size(); i ++) {
        //     print(_stdout, "ends ", i, ": { ");
        //     for (Location* l : frees[i]) print(_stdout, l->name, " ");
        //     println(_stdout, "}");
        // }

        for (u32 i = 0; i < insns.size(); i ++) {
            for (Location* l : frees[i]) {
                if (l->segm == REGISTER) {
                    // println(_stdout, REGISTER_NAMES[l->reg], " released from ", l->name);
                    if (l->reg >= XMM0)
                        fpavailable.insert(l->reg);
                    else available.insert(l->reg);
                }
            }
            for (Location* l : allocations[i]) {
                bool found = false;
                if (l->type->is<NumericType>() &&
                    l->type->as<NumericType>()->floating()) {
                    for (Register r : fporder) {
                        if (fpavailable.find(r) != fpavailable.end()) {
                            l->allocate(r);
                            // println(_stdout, l->name, " allocated to ", REGISTER_NAMES[r]);
                            fpavailable.erase(r);
                            found = true;
                            break;
                        }
                    }
                }
                else if (l->type->size() <= 8) {
                    for (Register r : order) {
                        if (available.find(r) != available.end()) {
                            l->allocate(r);
                            // println(_stdout, l->name, " allocated to ", REGISTER_NAMES[r]);
                            available.erase(r);
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) l->allocate(STACK, -i64(frame.slot(l->type)));
            }
        }
    }

    // Function

    Function::Function(const ustring& label):
        _stack(0), _temps(0), _label(label) {
        //
    }

    Location* Function::stack(const Type* type) {
        buffer b;
        ustring name;
        fprint(b, ".t", _temps ++);
        fread(b, name);
        return stack(type, name);
    }

    Location* Function::stack(const Type* type, const ustring& name) {
        variables.push(Location(type, name));
        return &variables.back();
    }

    u32 Function::slot(const Type* type) {
        if (_stack % type->size() != 0) // align
            _stack += type->size() - _stack % type->size();
        return _stack += type->size();
    }

    u32 Function::size() const {
        return _stack;
    }

    Insn* Function::add(Insn* i) {
        insns.push(i);
        if (i->is<Label>()) 
            labels.put(i->as<Label>()->label(), i->as<Label>());
        return insns.back();
    }
    
    Insn* Function::label(const ustring& name) {
        auto it = labels.find(name);
        if (it != labels.end()) return it->second;
        return nullptr;
    }

    void Function::allocate() {
        // hlCanonicalize(*this, insns);
        livenessPass(*this, insns);
        allocationPass(*this, insns);
        // for (Location& l : variables) {
        //     l.allocate(STACK, -i64(slot(l.type)));
        // }

        for (Location& l : variables) {
            if (l.segm == UNASSIGNED) l.kill();
        }
    }

    void Function::format(stream& io) {
        println(io, _label, ":");
        for (Insn* i : insns) i->format(io);
    }

    const ustring& Function::label() const {
        return _label;
    }

    // CodeGenerator

    CodeGenerator::CodeGenerator():
        _data(0), _label_ct(0), _stack(0), _temps(0), _datas(0) {
        //
    }

    Location* CodeGenerator::data(const Type* type, Data* src) {
        buffer b;
        fprint(b, ".g", _datas ++);
        ustring s;
        fread(b, s);
        datasrcs.push(src);
        datavars.push(Location(type, src, s));
        return &datavars.back();
    }

    Function* CodeGenerator::newFunction() {
        functions.push(new Function(newLabel()));
        return functions.back();
    }

    ustring CodeGenerator::newLabel() {
        buffer b;
        fprint(b, ".L", _label_ct ++);
        ustring s;
        fread(b, s);
        return s;
    }

    Location* CodeGenerator::stack(const Type* type) {
        buffer b;
        ustring name;
        fprint(b, ".t", _temps ++);
        fread(b, name);
        return stack(type, name);
    }

    Location* CodeGenerator::stack(const Type* type, const ustring& name) {
        variables.push(Location(type, name));
        return &variables.back();
    }

    u32 CodeGenerator::slot(const Type* type) {
        if (_stack % type->size() != 0) // align
            _stack += type->size() - _stack % type->size();
        return _stack += type->size();
    }

    Location* CodeGenerator::locateArg(const Type* type) {
        auto it = arglocs.find(type);
        if (it != arglocs.end()) return &it->second;
        if (type->size() <= 8) {
            if (type->is<NumericType>() && type->as<NumericType>()->floating())
                return &(arglocs[type] = Location(XMM0, type));
            return &(arglocs[type] = Location(RDI, type));
        }
        else {
            return &(arglocs[type] = Location(STACK, 16, type));
        }
    }

    Location* CodeGenerator::locateRet(const Type* type) {
        auto it = retlocs.find(type);
        if (it != retlocs.end()) return &it->second;
        if (type->size() <= 8) {
            if (type->is<NumericType>() && type->as<NumericType>()->floating())
                return &(retlocs[type] = Location(XMM0, type));
            return &(retlocs[type] = Location(RAX, type));
        }
        else {
            return &(retlocs[type] = Location(STACK, 16, type));
        }
    }

    u32 CodeGenerator::size() const {
        return _stack;
    }

    Insn* CodeGenerator::add(Insn* i) {
        insns.push(i);
        if (i->is<Label>()) 
            labels.put(i->as<Label>()->label(), i->as<Label>());
        return insns.back();
    }
    
    Insn* CodeGenerator::label(const ustring& name) {
        auto it = labels.find(name);
        if (it != labels.end()) return it->second;
        return nullptr;
    }

    void CodeGenerator::allocate() {
        for (Location& loc : datavars) {
            loc.allocate(DATA, _data);
            _data += loc.type->size();
        }

        for (Function* fn : functions) fn->allocate();

        livenessPass(*this, insns);
        allocationPass(*this, insns);
   
        // for (Location& l : variables) {
        //     l.allocate(STACK, -i64(slot(l.type)));
        // }

        for (Location& l : variables) {
            if (l.segm == UNASSIGNED) l.kill();
        }
    }

    void CodeGenerator::serialize() {
        //
    }

    void CodeGenerator::format(stream& io) {
        for (Function* f : functions) f->format(io);
        println(io, ".main:");
        for (Insn* i : insns) i->format(io);
    }

    // InsnClass

    InsnClass::InsnClass():
        _parent(nullptr) {
        //
    }

    InsnClass::InsnClass(const InsnClass& parent):
        _parent(&parent) {
        //
    }

    const InsnClass* InsnClass::parent() const {
        return _parent;
    }

    // Insn

    const InsnClass Insn::CLASS;

    Insn::Insn(const InsnClass* ic):
        _insnclass(ic), _cached(nullptr) {
        //
    }

    Location* Insn::value(CodeGenerator& gen) {
        return value(gen, gen);
    }

    Location* Insn::value(CodeGenerator& gen, CodeFrame& frame) {
        if (!_cached) _cached = lazyValue(gen, frame);
        return _cached;
    }
    
    const set<Location*>& Insn::inset() const {
        return _in;
    }
    
    const set<Location*>& Insn::outset() const {
        return _out;
    }
    
    bool Insn::liveout(CodeFrame& gen, const set<Location*>& out) {
        for (Location* l : out) _out.insert(l);
        _in = _out;
        return false;
    }

    // Data

    const InsnClass Data::CLASS(Insn::CLASS);

    Data::Data(const InsnClass* ic):
        Insn(ic), _generated(false) {
        //
    }

    // IntData

    map<i64, Location*> IntData::constants;

    const InsnClass IntData::CLASS(Data::CLASS);

    IntData::IntData(i64 value, const InsnClass* ic):
        Data(ic), _value(value) {
        //
    }

    Location* IntData::lazyValue(CodeGenerator& gen, CodeFrame& frame) {
        auto it = constants.find(_value);
        if (it != constants.end()) return it->second;
        else {
            _generated = true;
            return constants[_value] = gen.data(I64, this);
        }
    }

    i64 IntData::value() const {
        return _value;
    }

    const ustring& IntData::label() const {
        static ustring empty = "";
        return empty;
    }

    void IntData::format(stream& io) {
        //
    }

    void IntData::formatConst(stream& io) {
        print(io, _value);
    }

    // FloatData

    Location* FloatData::lazyValue(CodeGenerator& gen, CodeFrame& frame) {
        auto it = constants.find(_value);
        if (it == constants.end()) {
            _generated = true;
            constants[_value] = gen.data(DOUBLE, this);
        }
        return frame.stack(DOUBLE);
    }

    map<double, Location*> FloatData::constants;
    const InsnClass FloatData::CLASS(Data::CLASS);

    FloatData::FloatData(double value, const InsnClass* ic):
        Data(ic), _value(value) {
        //
    }

    double FloatData::value() const {
        return _value;
    }

    bool FloatData::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.erase(_cached);

        return false;
    }

    void FloatData::format(stream& io) {
        println(io, "    ", _cached, " = ", _value);
    }

    void FloatData::formatConst(stream& io) {
        print(io, _value);
    }

    const ustring& FloatData::label() const {
        return _cached->name;
    }

    // StrData

    map<ustring, Location*> StrData::constants;

    const InsnClass StrData::CLASS(Data::CLASS);

    StrData::StrData(const ustring& value, const InsnClass* ic):
        Data(ic), _value(value) {
        //
    }

    Location* StrData::lazyValue(CodeGenerator& gen, CodeFrame& frame) {
        auto it = constants.find(_value);
        if (it != constants.end()) return it->second;
        else {
            _generated = true;
            _label = gen.newLabel();
            return constants[_value] = gen.data(STRING, this);
        }
    }

    const ustring& StrData::value() const {
        return _value;
    }

    const ustring& StrData::label() const {
        return _label;
    }

    void StrData::format(stream& io) {
        //
    }

    void StrData::formatConst(stream& io) {
        print(io, '"', escape(_value), '"');
    }

    // BoolData

    Location* BoolData::lazyValue(CodeGenerator& gen, 
                                  CodeFrame& frame) {
        if (_value) {
            if (!TRUE) TRUE = gen.data(BOOL, this);
            return TRUE;
        }
        else {
            if (!FALSE) FALSE = gen.data(BOOL, this);
            return FALSE;
        }
    }

    Location *BoolData::TRUE = nullptr, *BoolData::FALSE = nullptr;
    
    const InsnClass BoolData::CLASS(Data::CLASS);
    
    BoolData::BoolData(bool value, const InsnClass* ic):
        Data(ic), _value(value) {
        //
    }

    bool BoolData::value() const {
        return _value;
    }

    void BoolData::format(stream& io) {
        //
    }

    void BoolData::formatConst(stream& io) {
        print(io, _value);
    }

    const ustring& BoolData::label() const {
        return _cached->name;
    }

    // BinaryInsn

    Location* BinaryInsn::lazyValue(CodeGenerator& gen, CodeFrame& frame) {
        return frame.stack(_lhs->type);
    }
    
    const InsnClass BinaryInsn::CLASS(Insn::CLASS);
    
    BinaryInsn::BinaryInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        Insn(ic), _lhs(lhs), _rhs(rhs) {
        //
    }

    bool BinaryInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.insert(_lhs);
        _in.insert(_rhs);
        _in.erase(_cached);
        return false;
    }

    // PrimaryInsn

    Location* PrimaryInsn::lazyValue(CodeGenerator& gen, 
                                     CodeFrame& frame) {
        return frame.stack(_operand->type);
    }

    const InsnClass PrimaryInsn::CLASS(Insn::CLASS);

    PrimaryInsn::PrimaryInsn(Location* operand, const InsnClass* ic):
        _operand(operand) {
        //
    }       
    
    bool PrimaryInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.insert(_operand);
        _in.erase(_cached);

        return false;
    }

    // AddInsn
    
    const InsnClass AddInsn::CLASS(BinaryInsn::CLASS);
    
    AddInsn::AddInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        BinaryInsn(lhs, rhs, ic) {
        //
    }

    void AddInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _lhs, " + ", _rhs);
    }

    // SubInsn
    
    const InsnClass SubInsn::CLASS(Insn::CLASS);
    
    SubInsn::SubInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        BinaryInsn(lhs, rhs, ic) {
        //
    }

    void SubInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _lhs, " - ", _rhs);
    }

    // MulInsn
    
    const InsnClass MulInsn::CLASS(Insn::CLASS);
    
    MulInsn::MulInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        BinaryInsn(lhs, rhs, ic) {
        //
    }

    void MulInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _lhs, " * ", _rhs);
    }

    // DivInsn
    
    const InsnClass DivInsn::CLASS(Insn::CLASS);
    
    DivInsn::DivInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        BinaryInsn(lhs, rhs, ic) {
        //
    }

    void DivInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _lhs, " / ", _rhs);
    }

    // ModInsn
    
    const InsnClass ModInsn::CLASS(Insn::CLASS);
    
    ModInsn::ModInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        BinaryInsn(lhs, rhs, ic) {
        //
    }

    void ModInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _lhs, " % ", _rhs);
    }

    // AndInsn

    const InsnClass AndInsn::CLASS(BinaryInsn::CLASS);

    AndInsn::AndInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        BinaryInsn(lhs, rhs, ic) {
        //
    }

    void AndInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _lhs, " and ", _rhs);
    }

    // OrInsn
    
    const InsnClass OrInsn::CLASS(BinaryInsn::CLASS);

    OrInsn::OrInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        BinaryInsn(lhs, rhs, ic) {
        //
    }

    void OrInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _lhs, " or ", _rhs);
    }

    // NotInsn

    const InsnClass NotInsn::CLASS(PrimaryInsn::CLASS);

    NotInsn::NotInsn(Location* operand, const InsnClass* ic):
        PrimaryInsn(operand, ic) {
        //
    }

    void NotInsn::format(stream& io) {
        println(io, "    ", _cached, " = not ", _operand);
    }

    // XorInsn

    const InsnClass XorInsn::CLASS(BinaryInsn::CLASS);

    XorInsn::XorInsn(Location* lhs, Location* rhs, const InsnClass* ic):
        BinaryInsn(lhs, rhs, ic) {
        //
    }

    void XorInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _lhs, " xor ", _rhs);
    }

    // JoinInsn

    Location* JoinInsn::lazyValue(CodeGenerator& gen, CodeFrame& frame) {
        return frame.stack(_result);
    }

    const InsnClass JoinInsn::CLASS(Insn::CLASS);

    JoinInsn::JoinInsn(const vector<Location*>& srcs, const Type* result, const InsnClass* ic):
        Insn(ic), _srcs(srcs), _result(result) {
        //
    }

    void JoinInsn::format(stream& io) {
        print(io, "    ", _cached, " = ");
        for (Location* src : _srcs) print(io, src == _srcs[0] ? "" : ", ", src);
        println("");
    }

    bool JoinInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        for (Location* src : _srcs)
            _in.insert(src);
        _in.erase(_cached);
        return false;
    }

    // FieldInsn

    Location* FieldInsn::lazyValue(CodeGenerator& gen, CodeFrame& frame) {
        return frame.stack(_src->type->as<TupleType>()->member(_index));
    }

    const InsnClass FieldInsn::CLASS(Insn::CLASS);

    FieldInsn::FieldInsn(Location* src, u32 index, const InsnClass* ic):
        Insn(ic), _src(src), _index(index) {
        //
    }

    void FieldInsn::format(stream& io) {
        print(io, "    ", _cached, " = ", _src, "[", _index, "]");
    }

    bool FieldInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.insert(_src);
        _in.erase(_cached);
        return false;
    }

    // CastInsn

    Location* CastInsn::lazyValue(CodeGenerator& gen, CodeFrame& frame) {
        return frame.stack(_target);
    }
    
    const InsnClass CastInsn::CLASS(Insn::CLASS);

    CastInsn::CastInsn(Location* src, const Type* target, const InsnClass* ic):
        Insn(ic), _src(src), _target(target) {
        //
    }

    void CastInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _src, " as ", _target);
    }

    bool CastInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.insert(_src);
        _in.erase(_cached);
        return false;
    }

    // GotoInsn

    Location* GotoInsn::lazyValue(CodeGenerator& gen, 
                                CodeFrame& frame) {
        return frame.none();
    }

    const InsnClass GotoInsn::CLASS(Insn::CLASS);

    GotoInsn::GotoInsn(const ustring& label, const InsnClass* ic):
        Insn(ic), _label(label) {
        //
    }

    bool GotoInsn::liveout(CodeFrame& frame, const set<Location*>& out) {
        u32 size = _out.size();
        const set<Location*>& path = frame.label(_label)->inset();
        for (Location* l : path) _out.insert(l);

        return size != _out.size();
    }
    
    void GotoInsn::format(stream& io) {
        println(io, "    goto ", _label);
    }

    // IfEqualInsn

    Location* IfEqualInsn::lazyValue(CodeGenerator& gen, 
                                CodeFrame& frame) {
        return frame.none();
    }

    const InsnClass IfEqualInsn::CLASS(Insn::CLASS);

    IfEqualInsn::IfEqualInsn(Location* lhs, Location* rhs, 
        const ustring& label, const InsnClass* ic):
        Insn(ic), _lhs(lhs), _rhs(rhs), _label(label) {
        //
    }
    
    void IfEqualInsn::format(stream& io) {
        println(io, "    if ", _lhs, " == ", _rhs, ": goto ", _label);
    }

    bool IfEqualInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        u32 size = _out.size();
        const set<Location*>& path = gen.label(_label)->inset();
        
        for (Location* l : out) _out.insert(l);
        for (Location* l : path) _out.insert(l);
        _in = _out;
        _in.insert(_lhs);
        _in.insert(_rhs);

        return _out.size() != size;
    }

    // CallInsn

    Location* CallInsn::lazyValue(CodeGenerator& gen, 
                                CodeFrame& frame) {
        if (_func->type->as<FunctionType>()->ret() == VOID) return frame.none();
        return frame.stack(_func->type->as<FunctionType>()->ret());
    }

    const InsnClass CallInsn::CLASS(Insn::CLASS);

    CallInsn::CallInsn(Location* operand, Location* func, const InsnClass* ic):
        Insn(ic), _operand(operand), _func(func) {
        //
    }

    bool CallInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.insert(_func);
        _in.insert(_operand);
        _in.erase(_cached);

        return false;
    }
    
    void CallInsn::format(stream& io) {
        println(io, "    ", _cached, " = ", _func, " (", _operand, ")");
    }

    // RetInsn

    Location* RetInsn::lazyValue(CodeGenerator& gen, 
                                CodeFrame& frame) {
        return frame.none();
    }

    const InsnClass RetInsn::CLASS(Insn::CLASS);

    RetInsn::RetInsn(Location* operand, const InsnClass* ic):
        Insn(ic), _operand(operand) {
        //
    }

    bool RetInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.insert(_operand);

        return false;
    }
    
    void RetInsn::format(stream& io) {
        println(io, "    ", "return ", _operand);
    }

    // MovInsn
    
    Location* MovInsn::lazyValue(CodeGenerator& gen,
                                CodeFrame& frame) {
        return frame.none();
    }

    InsnClass MovInsn::CLASS(Insn::CLASS);

    MovInsn::MovInsn(Location* dst, Location* src, const InsnClass* ic):
        Insn(ic), _dst(dst), _src(src) {
        //
    }

    bool MovInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.insert(_src);
        _in.erase(_dst);

        return false;
    }

    void MovInsn::format(stream& io) {
        println(io, "    ", _dst, " = ", _src);
    }
    
    Location* MovInsn::dst() const {
        return _dst;
    }

    Location*& MovInsn::dst() {
        return _dst;
    }

    Location* MovInsn::src() const {
        return _src;
    }

    Location*& MovInsn::src() {
        return _src;
    }

    // LeaInsn
    
    Location* LeaInsn::lazyValue(CodeGenerator& gen,
                                CodeFrame& frame) {
        return frame.none();
    }

    InsnClass LeaInsn::CLASS(Insn::CLASS);

    LeaInsn::LeaInsn(Location* dst, const ustring& label, const InsnClass* ic):
        Insn(ic), _dst(dst), _label(label) {
        //
    }

    bool LeaInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.erase(_dst);

        return false;
    }

    void LeaInsn::format(stream& io) {
        println(io, "    ", _dst, " = &", _label);
    }

    // PrintInsn
    
    Location* PrintInsn::lazyValue(CodeGenerator& gen,
                                CodeFrame& frame) {
        return frame.none();
    }

    InsnClass PrintInsn::CLASS(Insn::CLASS);

    PrintInsn::PrintInsn(Location* src, const InsnClass* ic):
        Insn(ic), _src(src) {
        //
    }

    bool PrintInsn::liveout(CodeFrame& gen, const set<Location*>& out) {
        Insn::liveout(gen, out);
        _in.insert(_src);

        return false;
    }

    void PrintInsn::format(stream& io) {
        println(io, "    print ", _src);
    }

    // Label
    
    Location* Label::lazyValue(CodeGenerator& gen,
                              CodeFrame& frame) {
        return frame.none();
    }

    InsnClass Label::CLASS(Insn::CLASS);

    Label::Label(const ustring& label, bool global, const InsnClass* ic):
        Insn(ic), _label(label), _global(global) {
        //
    }

    const ustring& Label::label() const {
        return _label;
    }

    void Label::format(stream& io) {
        println(io, _label, ":");
    }
}

void print(stream& s, basil::Location* loc) {
    if (loc->imm) loc->imm->formatConst(s);
    else if (loc->segm == basil::RELATIVE) print(s, loc->src->name, ".", loc->name);
    else if (loc->name.size()) print(s, loc->name);
    else if (loc->segm == basil::REGISTER) print(s, basil::REGISTER_NAMES[loc->reg]);
}

void print(basil::Location* loc) {
    print(_stdout, loc);
}

namespace basil {
    // x86_64 Target

    void Function::emitX86(buffer& text, buffer& data) {
        x64::printer::label(text, data, x64::TEXT, _label, false);

        u32 i = 0;
        for (; i < insns.size() && insns[i]->is<Label>(); i ++) 
            insns[i]->emitX86(text, data);

        Location rbp(RBP, I64);
        Location rsp(RSP, I64);
        Location frame(size());

        if (size() > 0) {
            x64::printer::push(text, data, &rbp);
            x64::printer::mov(text, data, &rsp, &rbp);
            x64::printer::sub(text, data, &frame, &rsp);
        }

        for (; i < insns.size(); i ++) insns[i]->emitX86(text, data);

        if (size() > 0) {
            x64::printer::mov(text, data, &rbp, &rsp);
            x64::printer::pop(text, data, &rbp);
        }

        x64::printer::ret(text, data);
    }

    void CodeGenerator::emitX86(buffer& text, buffer& data) {
        x64::printer::data(text, data);
        for (Data* d : datasrcs) d->emitX86Const(text, data);
        x64::printer::text(text, data);
        for (Function* f : functions) f->emitX86(text, data);
        x64::printer::label(text, data, x64::TEXT, "_start", true);
        
        Location rax(RAX, I64);
        Location rdi(RDI, I64);
        Location rbp(RBP, I64);
        Location rsp(RSP, I64);
        Location frame(size());
        Location exit(60);
        Location code(0);

        if (size() > 0) {
            x64::printer::mov(text, data, &rsp, &rbp);
            x64::printer::sub(text, data, &frame, &rsp);
        }

        for (Insn* i : insns) i->emitX86(text, data);
        x64::printer::mov(text, data, &exit, &rax);
        x64::printer::mov(text, data, &code, &rdi);
        x64::printer::syscall(text, data);
    }

    void Insn::emitX86(buffer& text, buffer& data) {
        //
    }

    void IntData::emitX86Const(buffer& text, buffer& data) {
        x64::printer::intconst(text, data, _value);
    }

    void IntData::emitX86Arg(buffer& text) {
        fprint(text, "$", _value);
    }
    
    void FloatData::emitX86Const(buffer& text, buffer& data) {
        x64::printer::label(text, data, x64::DATA, constants[_value]->name, false);
        x64::printer::fconst(text, data, _value);
    }

    void FloatData::emitX86(buffer& text, buffer& data) {
        x64::printer::mov(text, data, constants[_value], _cached);
    }

    void FloatData::emitX86Arg(buffer& text) {
        fprint(text, constants[_value]->name);
    }

    void StrData::emitX86Const(buffer& text, buffer& data) {
        u32 len = 0;
        for (u32 i = 0; i < _value.size(); i ++) len += _value[i].size();
        x64::printer::label(text, data, x64::DATA, _label, false);
        x64::printer::intconst(text, data, len);
        x64::printer::strconst(text, data, _value);
    }

    void StrData::emitX86Arg(buffer& text) {
        fprint(text, "$", _label);
    }

    void BoolData::emitX86Const(buffer& text, buffer& data) {
        x64::printer::intconst(text, data, _value ? 1 : 0);
    }

    void BoolData::emitX86Arg(buffer& text) {
        fprint(text, "$", _value ? 1 : 0);
    }

    void AddInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        Location *first = _lhs, *second = _rhs;
        if (*second == *_cached) {
            second = _lhs;
            first = _rhs;
        }
        x64::printer::mov(text, data, first, _cached);
        x64::printer::add(text, data, second, _cached);
    }

    void SubInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        Location *first = _lhs, *second = _rhs;
        if (*second == *_cached) {
            second = _lhs;
            first = _rhs;
        }
        x64::printer::mov(text, data, first, _cached);
        x64::printer::sub(text, data, second, _cached);
    }

    void MulInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        Location *first = _lhs, *second = _rhs;
        if (*second == *_cached) {
            second = _lhs;
            first = _rhs;
        }
        x64::printer::mov(text, data, first, _cached);
        if (_cached->type->is<NumericType>() 
            && (_cached->type->as<NumericType>()->floating()
                || !_cached->type->as<NumericType>()->isSigned()))
            x64::printer::mul(text, data, second, _cached);
        else x64::printer::imul(text, data, second, _cached);
    }

    void DivInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        Location rax(RAX, _lhs->type);
        Location rdx(RDX, _lhs->type);
        Location *first = _lhs, *second = _rhs;
        if (*second == *_cached) {
            second = _lhs;
            first = _rhs;
        }

        x64::printer::mov(text, data, first, &rax);
        x64::printer::xor_(text, data, &rdx, &rdx);
        x64::printer::idiv(text, data, second);
        x64::printer::mov(text, data, &rax, _cached);
    }

    void ModInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        Location rax(RAX, _lhs->type);
        Location rdx(RDX, _lhs->type);
        Location *first = _lhs, *second = _rhs;
        if (*second == *_cached) {
            second = _lhs;
            first = _rhs;
        }

        x64::printer::mov(text, data, first, &rax);
        x64::printer::xor_(text, data, &rdx, &rdx);
        x64::printer::idiv(text, data, second);
        x64::printer::mov(text, data, &rdx, _cached);
    }

    void AndInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        Location *first = _lhs, *second = _rhs;
        if (*second == *_cached) {
            second = _lhs;
            first = _rhs;
        }
        x64::printer::mov(text, data, first, _cached);
        x64::printer::and_(text, data, second, _cached);
    }

    void OrInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        Location *first = _lhs, *second = _rhs;
        if (*second == *_cached) {
            second = _lhs;
            first = _rhs;
        }
        x64::printer::mov(text, data, first, _cached);
        x64::printer::or_(text, data, second, _cached);
    }

    void XorInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        Location *first = _lhs, *second = _rhs;
        if (*second == *_cached) {
            second = _lhs;
            first = _rhs;
        }
        x64::printer::mov(text, data, first, _cached);
        x64::printer::xor_(text, data, second, _cached);
    }

    void NotInsn::emitX86(buffer& text, buffer& data) {
        if (!*_cached) return;
        x64::printer::mov(text, data, _operand, _cached);
        x64::printer::not_(text, data, _cached);
    }

    void JoinInsn::emitX86(buffer& text, buffer& data) {
        i64 offset = 0;
        for (Location* l : _srcs) {
            Location off = Location(l->type, _cached, offset, "");
            x64::printer::mov(text, data, l, &off);
            offset += l->type->size();
        }
    }

    void FieldInsn::emitX86(buffer& text, buffer& data) {
        const TupleType* src = _src->type->as<TupleType>();
        Location off = Location(src->member(_index), _src, src->offset(_index), "");
        x64::printer::mov(text, data, &off, _cached);
    }

    void LeaInsn::emitX86(buffer& text, buffer& data) {
        if (!*_dst) return;
        x64::printer::lea(text, data, _label, _dst);
    }

    void MovInsn::emitX86(buffer& text, buffer& data) {
        if (!*_dst) return;
        x64::printer::mov(text, data, _src, _dst);
    }

    void RetInsn::emitX86(buffer& text, buffer& data) {
        Location rax(RAX, _operand->type);
        x64::printer::mov(text, data, _operand, &rax);
    }
    
    void CastInsn::emitX86(buffer& text, buffer& data) {
        if (_src->type->is<NumericType>() && _target->is<NumericType>()) {
            const NumericType* st = _src->type->as<NumericType>();
            const NumericType* nt = _target->as<NumericType>();
            if (st->floating() && !nt->floating()) {
                if (st->size() == 8) 
                    x64::printer::cvttsd2si(text, data, _src, _cached);
                if (st->size() == 4) 
                    x64::printer::cvttss2si(text, data, _src, _cached);
            }
            else if (!st->floating() && nt->floating()) {
                if (nt->size() == 8) 
                    x64::printer::cvtsi2sd(text, data, _src, _cached);
                if (nt->size() == 4) 
                    x64::printer::cvtsi2ss(text, data, _src, _cached);
            }
            else if (st->floating() && nt->floating()) {
                if (st->size() == 4 && nt->size() == 8) 
                    x64::printer::cvtss2sd(text, data, _src, _cached);
                if (st->size() == 8 && nt->size() == 4)
                    x64::printer::cvtsd2ss(text, data, _src, _cached);
            }
            else {
                if (st->size() >= nt->size()) 
                    x64::printer::mov(text, data, _src, _cached);
                else if (nt->isSigned())
                    x64::printer::movsx(text, data, _src, _cached);
                else
                    x64::printer::movzx(text, data, _src, _cached);
            }
        }

        // TODO: other casts
    }

    void GotoInsn::emitX86(buffer& text, buffer& data) {
        x64::printer::jmp(text, data, _label);
    }

    void IfEqualInsn::emitX86(buffer& text, buffer& data) {
        x64::printer::cmp(text, data, _lhs, _rhs);
        x64::printer::jcc(text, data, _label, x64::EQUAL);
    }

    void CallInsn::emitX86(buffer& text, buffer& data) {
        Location rax(RAX, _operand->type);
        Location rdi(RDI, _operand->type);
        Location rsi(RSI, _operand->type);
        // const FunctionType* ft = _func->type->as<FunctionType>();

        vector<Location*> saved;
        for (Location* l : _in) if (l->segm == REGISTER) {
            if (_out.find(l) != _out.end()) saved.push(l);
        }

        for (u32 i = 0; i < saved.size(); i ++) {
            x64::printer::push(text, data, saved[i]);
        }
        
        x64::printer::mov(text, data, _operand, &rdi);
        x64::printer::call(text, data, _func);
        if (*_cached) x64::printer::mov(text, data, &rax, _cached);

        for (i64 i = i64(saved.size()) - 1; i >= 0; i --) {
            x64::printer::pop(text, data, saved[i]);
        }
    }

    void PrintInsn::emitX86(buffer& text, buffer& data) {
        Location rax(RAX, _src->type);
        Location rdx(RDX, _src->type);
        Location rsi(RSI, _src->type);
        Location rdi(RDI, _src->type);
        Location size(RAX, 0, I64);
        Location body(RAX, 8, STRING);
        Location one(1);

        x64::printer::mov(text, data, _src, &rax);
        x64::printer::mov(text, data, &size, &rdx);
        x64::printer::lea(text, data, &body, &rsi);
        x64::printer::mov(text, data, &one, &rax);
        x64::printer::xor_(text, data, &rdi, &rdi);
        x64::printer::syscall(text, data);
    }

    void Label::emitX86(buffer& text, buffer& data) {
        x64::printer::label(text, data, x64::TEXT, _label, _global);
    }
}