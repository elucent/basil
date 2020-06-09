#ifndef BASIL_DEFS_H
#define BASIL_DEFS_H

#include <cstdint>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// ir.h

namespace basil {
    struct Location;
    class CodeFrame;
    class Function;
    class CodeGenerator;
    class InsnClass;
    class Insn;
    class Data;
    class IntData;
    class FloatData;
    class CharData;
    class StringData;
    class AddInsn;
    class SubInsn;
    class MulInsn;
    class DivInsn;
    class ModInsn;
    class FAddInsn;
    class FSubInsn;
    class FMulInsn;
    class FDivInsn;
    class FModInsn;
    class AndInsn;
    class OrInsn;
    class NotInsn;
    class XorInsn;
    class EqInsn;
    class NotEqInsn;
    class LessInsn;
    class GreaterInsn;
    class LessEqInsn;
    class GreaterEqInsn;
    class GotoInsn;
    class CallInsn;
    class RetInsn;
    class MovInsn;
    class LeaInsn;
    class Label;
}

// hash.h

template<typename T>
class set;

template<typename K, typename V>
class map;

// io.h

class stream;
class file;
class buffer;

// str.h

class string;

// utf8.h

struct uchar;
class ustring;

// vec.h

template<typename T>
class vector;

namespace basil {
    
    // src.h

    class Source;

    // error.h
    
    struct Error;

    // lex.h

    struct Token;
    class TokenCache;

    // type.h

    class TypeClass;
    class Type;
    class NumericType;
    class TupleType;
    class UnionType;
    class IntersectionType;
    class FunctionType;

    // import.h

    class Module;

    // meta.h
    class Meta;
    class MetaRC;
    class MetaString;
    class MetaList;
    class MetaTuple;
    class MetaArray;
    class MetaUnion;
    class MetaIntersect;
    class MetaFunction;

    // term.h

    class TermClass;
    class Term;
    class IntegerTerm;
    class RationalTerm;
    class StringTerm;
    class CharTerm;
    class VariableTerm;
    class BlockTerm;
    class LambdaTerm;
    class AssignTerm;
    class ProgramTerm;

    // value.h

    class ValueClass;
    class Stack;
    class Value;
    class IntegerConstant;
    class RationalConstant;
    class StringConstant;
    class CharConstant;
    class TypeConstant;
    class Variable;
    class Block;
    class Lambda;
    class Program;
    class BinaryOp;
    class BinaryMath;
    class Add;
    class Subtract;
    class Multiply;
    class Divide;
    class Modulus;
    class BinaryLogic;
    class And;
    class Or;
    class Xor;
    class Not;
    class BinaryEquality;
    class Equal;
    class Inequal;
    class BinaryRelation;
    class Less;
    class LessEqual;
    class Greater;
    class GreaterEqual;
    class Join;
    class Intersect;
    class Define;
    class Autodefine;
    class Assign;
    class Cast;
    class Eval;
}

#endif