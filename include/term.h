#ifndef BASIL_TERM_H
#define BASIL_TERM_H

#include "defs.h"
#include "vec.h"
#include "utf8.h"

namespace basil {
    class TermClass {
        const TermClass* _parent;
    public:
        TermClass();
        TermClass(const TermClass& parent);
        const TermClass* parent() const;
    };

    class Term {
        u32 _line, _column;
        Term* _parent;
        const TermClass* _termclass;
    protected:
        vector<Term*> _children;
        void indent(stream& io, u32 level) const;
    public:
        static const TermClass CLASS;

        Term(u32 line, u32 column, const TermClass* tc = &CLASS);
        virtual ~Term();
        u32 line() const;
        u32 column() const;
        void setParent(Term* parent);
        virtual void format(stream& io, u32 level = 0) const = 0;
        virtual void eval(Stack& stack) = 0;
        virtual bool equals(const Term* other) const = 0;
        virtual u64 hash() const = 0;
        virtual Term* clone() const = 0;

        template<typename T>
        bool is() const {
            const TermClass* ptr = _termclass;
            while (ptr) {
                if (ptr == &T::CLASS) return true;
                ptr = ptr->parent();
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

        template<typename Func>
        void foreach(const Func& func) {
            func(this);
            for (Term* v : _children) v->foreach(func);
        }
    };

    class IntegerTerm : public Term {
        i64 _value;
    public:
        static const TermClass CLASS;

        IntegerTerm(i64 value, u32 line, u32 column, 
                    const TermClass* tc = &CLASS);
        i64 value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class RationalTerm : public Term {
        double _value;
    public:
        static const TermClass CLASS;

        RationalTerm(double value, u32 line, u32 column,
                     const TermClass* tc = &CLASS);
        double value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class StringTerm : public Term {
        ustring _value;
    public:
        static const TermClass CLASS;

        StringTerm(const ustring& value, u32 line, u32 column,
                   const TermClass* tc = &CLASS);
        const ustring& value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class CharTerm : public Term {
        uchar _value;
    public:
        static const TermClass CLASS;

        CharTerm(uchar value, u32 line, u32 column,
                 const TermClass* tc = &CLASS);
        uchar value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class BoolTerm : public Term {
        bool _value;
    public:
        static const TermClass CLASS;

        BoolTerm(bool value, u32 line, u32 column,
                 const TermClass* tc = &CLASS);
        bool value() const;
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class VoidTerm : public Term {
    public:
        static const TermClass CLASS;

        VoidTerm(u32 line, u32 column, const TermClass* tc = &CLASS);
        
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class EmptyTerm : public Term {
    public:
        static const TermClass CLASS;

        EmptyTerm(u32 line, u32 column, const TermClass* tc = &CLASS);
        
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class VariableTerm : public Term {
        ustring _name;
    public:
        static const TermClass CLASS;

        VariableTerm(const ustring& name, u32 line, u32 column,
                     const TermClass* tc = &CLASS);
        const ustring& name() const;
        void rename(const ustring& name);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class BlockTerm : public Term {
    public:
        static const TermClass CLASS;

        BlockTerm(const vector<Term*>& children, u32 line, u32 column,
                  const TermClass* tc = &CLASS);
        const vector<Term*>& children() const;
        void add(Term* child);
        virtual void format(stream& io, u32 level = 0) const override;
        virtual void eval(Stack& stack) override;
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };

    class ProgramTerm : public Term {
        Stack *root, *global;

        void initRoot();
    public:
        static const TermClass CLASS;

        ProgramTerm(const vector<Term*>& children, u32 line, u32 column,
                    const TermClass* tc = &CLASS);
        ~ProgramTerm();
        const vector<Term*>& children() const;
        void add(Term* child);  
        virtual void format(stream& io, u32 level = 0) const override;  
        virtual void eval(Stack& stack) override;
        void evalChild(Stack& stack, Term* t);
        Stack& scope();
        virtual bool equals(const Term* other) const override;
        virtual u64 hash() const override;
        virtual Term* clone() const override;
    };
}

#endif