# The Basil Programming Language

---

# 1 - Overview

## 1.1 - Language Summary

Basil is a strongly and statically typed language, with a stack-based evaluation model, lexical scoping, and features for compile-time evaluation.

### Code Structure

All code in Basil is comprised of lists of terms, known as _blocks_. Blocks themselves _are_ terms, and thus can be nested arbitrarily, creating a program's structure. This is similar to languages in the Lisp family, although Basil supports quite a few different ways of grouping terms into blocks:
```rb
(A B C)     -> (A B C)
{A B C}     -> (A B C)
A: B C      -> A (B C)
A; B C      -> (A) (B C)
A . B C     -> (A B) C
A:          
    B C     -> A (B C)
```
This syntactic variation allows code to be styled quite freely, and despite sharing a structural similarity with the Lisp family, Basil source code more visually resembles C-like and ML-like languages.

### Evaluation

Terms are evaluated using a stack. Each time a term is evaluated, it may push one or more values onto the stack. If a function is pushed on the stack, it may try to bind to the top value of the stack and produce a new value. If a function is already on top of the stack, it can try to bind to new values being pushed on. In this way, Basil implements a very freeform evaluation model where functions may freely be called prefix, infix, or postfix:
```rb
1 2 +       -> 3         
1 + 2       -> 3
+ 1 2       -> 3
```

Blocks are used to group evaluation. The contents of a block are evaluated completely before they are pushed, one by one, to the enclosing block. This allows the separation of different expressions and the expression of order of operations:
```rb
(1 + 2) * 3                     -> 9
print "hello"; print " world"   -> hello world
```

### First-Class Everything

Everything in Basil other than the delimiters used to group blocks together is a value with a type. All values can be passed into functions, returned from functions, and be assigned to variables.

This includes traditional data types:
```rb
int i = 1   -> i = 1
i = i * 5   -> i = 5
```
...functions themselves (including `->` syntactic sugar for anonymous functions):
```rb
let f x -> x + 1
f 10        -> 11
```
...as well as types:
```rb
let vec3(type t) -> t, t, t
vec3(int) v = 1, 2, 3
```
...and even the built-in operations of the language:
```rb
let var = let
var plus = +
var x = 1 plus 2 plus 3     -> x = 6
```

### Metaprogramming with Macros

Basil includes a simple but versatile macro system to directly modify the evaluation stack. Macros can bind with values on the stack just like functions. Instead of being called, however, macros will splice their arguments into a block and push the resulting values back onto the stack directly. This allows for compile-time functions as well as some interesting stack-based operations:
```rb
let dup x -< x x            -> new macro "dup"
dup 1                       -> 1 1
dup 2 *                     -> 2 * 2            -> 4
```

### Homoiconicity

Basil can also omit terms from evaluation via _quoting_. Both functions and macros can be specified to automatically quote their argument, essentially treating a term as a value that can be evaluated later. All quoted code is typed, and can be used in expressions on its own or evaluated explicitly.
```rb
let expr = [1 + 2]          -> expr = (quote (1 + 2))
!expr + 3                   -> 6

let void-fn body => {       -> new quoting function "void-fn"
    () -> !body
}
void-fn(print "hi")         -> () -> print "hi"
```

## 1.2 - Why Basil?

There are lots of programming languages out there. Why use this one?

Basil's genesis came from my frustration with the cruft that comes with highly-structured, statement-based languages like C++ and Java. I have nothing against C++ or Java, and I use both fairly regularly for different projects, but their adherence to strict grammatical rules can often get in the way of development.

For example, consider Java's requirement that all code be contained within a class. Java is very object-oriented, so this makes sense with regard to much of its standard library, and the type of inheritance-driven code that most Java work consists of.

But what if you just want one function? Say you'd like to make a utility that computes the dot product of two arrays. This doesn't depend on inheritance, or on member variables external to the function. The most straightforward implementation simply takes in two arrays and returns a number, and gains no benefit from being in a class. But, Java still requires that the function be wrapped in a class. Most often, this leads to the creation of static "utility classes", with many static member variables and functions. Placing functions in a class like this doesn't much change their meaning to the programmer - other than the class serving the role of a namespace, possibly clarifying that a function belongs to a particular set of utilities. Certainly, marking every function as `public` and `static` doesn't contribute much meaning - we just want a function! If it's clear that the function makes no particular use of member variables or access levels, `public` and `static` _mostly_ just exist to satisfy the compiler, not inform the programmer. There's a number of other arguments you can make about the philosophy here, but I personally consider this a case where a language's _strict structure_ leads to a lot of meaningless work creating organizational classes and adding likely-unnecessary keywords.

The more I ran into cases like these, the more I thought about their commonalities. While the requirement to place functions in a class is fairly Java-specific, structural rules getting in the way of programmar intent is something that shows up in almost any language. As another example, let's consider C++ containers. To properly define iterators for a container in C++, both an `iterator` type and a `const_iterator` type must generally be created. The two iterators behave in an almost identical way, the only difference being that one guarantees constness and the other doesn't. It's possible to resolve this issue somewhat using templates, but this complicates the `iterator` type considerably. It's pretty common in my experience to just let this duplication occur. But it's also very clear to me when I run into these situations that only a simple change is needed to get the type I want - making _this_ variable const, making _this_ operator const, etc. The wall I'm running into isn't a conceptual one, the language just isn't capable of expressing the simplest solution elegantly - or sometimes at all.

I think languages in the Lisp family, certain dynamic languages like Ruby, or functional languages like Haskell and ML do a better job at this. Lisps especially, and in particular simpler Lisps such as Scheme, hold exceptional expressive power with a tiny amount of syntax. Macros blur the line between procedures and syntax, and I've often heard the Lisp school of problem-solving described as first "designing a language to express the problem". For each of these language categories, there are specific features you can point to that help make the language's syntax more versatile - Ruby supports lots of dynamic behavior, Haskell does fairly intelligent type inference, Scheme can interchange source code and data and uses the same expressions for each. 

But what I personally think is the unifying thread for why all of these features help language versatility is this: they reduce the number of different distinct concepts in the language. If I don't need to specify as many distinct characteristics of my code in order for it to work, there's less information crowding the actual behavior in my program. But even more broadly and (in my opinion) importantly, if more and more aspects of my language operate the same way, I can reuse my patterns and abstractions more generally across the language. If I want to create a few distinct but similar classes in C++, I need to transform the base class structure into a template, and specialize it as necessary for each variant. If I want to take a class in Ruby and duplicate it and modify its methods, I can do that as if I was copying or modifying any other object. In Ruby, I could even write a function to do this for me. In C++, where the entire process is sequestered from the rest of the language and uses its own specific mechanics, I can't.

To make a long story short, Basil is the result of trying to create a language that emphasizes this kind of flexibility. I wanted to make a language with simple yet freeform syntax, where every language feature was a value, with a type. Where types and functions and code could be played with, using the same basic concepts you'd use to manipulate numbers or strings. And I wanted to try and do this without sacrificing expressiveness, simplicity, familiarity, or performance. The results of this project thus far are what comprise this specification.

## 1.3 - Core Design Principles

Basil's design is based around a couple of general guiding principles. In this section, these principles are listed, roughly in order of importance.

### Flexibility

As discussed previously, Basil's _primary goal_ is to be an extremely flexible language. Freedom in writing code helps to foster creative and unique solutions to problems. Basil should strive to never get in the way when writing an abstraction or expressing an algorithm effectively. This generally manifests in Basil trying to minimize rules and restrictions on how code can be written and what can be done where. 

### Simplicity

Basil also strives to stay simple. Dozens of keywords and extremely complicated types can make a language hard to learn - Basil should attempt to do the opposite. As much as possible, rely on existing ideas, concepts, and features to implement functionality. This primarily manifests in Basil's simple structure and evaluation procedure, and how relatively few built-in operations stray from what's possible for a user to do.

### Performance

While expressiveness and simplicity are the driving goals of Basil as a language, it's also important that Basil keeps performance in mind. With full static typing and a simple grammar and evaluation model, there's no reason why Basil can't pull off respectable performance in an optimized implementation.

### Familiarity

In the face of its somewhat unconventional systems, Basil should try and be friendly and familiar to those experienced in other programming languages. To the greatest reasonable extent, Basil should try to support conventional syntactic patterns, statements, and expressions, to bridge the gap between other languages and Basil's idiosyncrasies.

---

# 2 - Lexical Structure

## 2.1 - Separators

In Basil, tokens are separated by _delimiters_, represented by any of the following characters:
 * Parentheses (`()`)
 * Curly braces (`{}`)
 * Square brackets (`[]`)
 * Period (`.`)
 * Comma (`,`)
 * Semicolon (`;`)

_Whitespace_ characters - spaces, tabs, or newlines, are also delimiters. A _colon_ (`:`)
followed by any whitespace character is also a delimiter, provided it is not preceded by another colon.

All Basil sources end in a newline token. If a source file doesn't end in a newline, Basil will add one during compilation.

## 2.2 - Constants

There are six types of constants represented in Basil's syntax.

A _character constant_ is any single character (other than a newline or a single quote (`'`)) enclosed in single quotes. It's also acceptable to replace this single character with a valid _escape sequence_, which corresponds to an otherwise unrepresentable character as listed below:
 * `\n` - Line Feed
 * `\r` - Carriage Return
 * `\t` - Tab
 * `\0` - Null
 * `\\` - Backslash
 * `\"` - Double Quote
 * `\'` - Single Quote

A _string constant_ consists of any number of characters or escape sequences (other than newlines or double quotes (`"`)) enclosed in double quotes.

An _integer constant_ is one or more base-10 digits, followed by a delimiter. 

A _rational constant_ is any integer constant, followed by a decimal point (`.`), followed by another integer constant.

A _symbol constant_ is any identifer preceded by a hash (`#`).

Finally, a _boolean constant_ is either the string `true` or `false`, followed by a delimiter.

```rb
\ examples of valid constants

'a' '\t' '1' ' '
"hello" "1 + 2" "dog\n" ""
3 59
5.4 0.01
#red #green #blue
true false
```

## 2.3 - Prefixes

A _prefix_ is one of several built-in symbols Basil defines as syntactic sugar to cut down on unnecessary whitespace in code. Prefixes are represented by single characters, and each valid prefix character and associated operation is listed below:
 * `-` - Minus
 * `+` - Plus
 * `!` - Evaluate
 * `~` - Reference

These four characters yield unique tokens, even if no delimiter separates them from the next token, unless followed by another prefix character, `<`, `>`, or `=`.

```rb
\ valid prefix tokens
-1 +24 !x ~y

\ not prefix tokens
-- ++ != ~>
```

## 2.4 - Operators

An _operator_ is one of several built-in symbols Basil defines as syntactic sugar for common operations. Operators consist of an exact character sequence, followed by a delimiter. Valid character sequences and their associated operations are listed below:
 * `->` - Lambda
 * `=>` - Metalambda
 * `-<` - Macro
 * `=<` - Metamacro
 * `=` - Assign
 * `:=` - Define

## 2.5 - Identifiers

An _identifier_ is any sequence of one or more characters, followed by a delimiter, provided that:
 * The sequence contains only _symbolic_ characters - this includes any UTF-8 character that is not whitespace, a control character, or a delimiter.
 * The sequence does not match any other token described by this specification.
 * The sequence does not begin with a digit or an underscore (`_`).

Additionally, any sequence of two or more consecutive period (`.`) characters is considered an identifier.

```py
\ example identifiers

abc test_function x
group01 x2_3
+-+ $$ %= ::
α β γ δ
...
```

## 2.6 - Comments

A _comment_ begins with a backslash (`\`) character. All characters in a line after and including the backslash, but not including the line feed at the end of the line, are ignored.

Backslash characters within character and string literals do not begin comments.

---

# 3 - Grammar

## 3.1 - Syntax Overview

Basil programs are comprised of _terms_. A term is a piece of code that can _evaluate_. This includes every kind of constant, as well as identifiers. In addition to these, we also define the _void constant_ and _empty constant_, `()` and `[]` respectively.

A _block_ of terms is also a term. A block is just a sequence of one or more terms, used to group terms together. A block is fundamentally written as any number of other terms enclosed within parentheses.

```rb
\ example single terms

1 2.0 true #cat "hello" 'a' () [] my-variable

\ example block

(+ 1 2)
```

Basil supports a number of syntactic equivalences to help decrease the parenthese count when writing code with many nested blocks. These includes the introduction of a syntactic hierarchy between several kinds of blocks:
 * An _enclosed block_, as described above, can be said to contain one or more _lines_.
 * A _line_ is a sequence that contains one or more _phrases_.
 * A _phrase_ is a sequence that contains one or more _terms_.

Each of these types of block is still a block, containing one or more terms. This hierarchy is involved only when a block is _subdivided_ syntactically. A block can only be divided into phrases within a line. And a block can only be divided into lines within an enclosed block.

This might sound complicated, but in essence, it means we create fewer unnecessary blocks. Even though `(1 2 3)` is an enclosed block, it's still just a block of three terms. Only if we subdivide it, for example `(1 2; 3)` does the block structure get more complicated - in this case, our block contains a block of two elements (`1 2`), and a block of one element (`3`).

With that in mind, here is a list of the structural equivalences Basil supports:

| Syntax                | Equivalent                    | Description                                                               |
|---                    |---                            |---                                                                        |
| `{A B C}`             | `(A B C)`                     | Curly braces serve as an alternative to parentheses.                      |
| `A; B; C`             | `(A) (B) (C)`                 | Semicolons group terms into phrases.                                      |
| `A B <newline> C`     | `(A B) (C)`                   | Newlines group terms into lines.                                          |
| `A: B; C`             | `(A (B)) (C)`                 | Colons group terms after them into a phrase.                              |
| `A . B C`             | `(A B) C`                     | Dots group adjacent terms together.                                       |
| `A: <newline> <indent> B C <newline> <dedent>` | `A (B C)` | Groups indented code into an enclosed block.                         |

There's still one area of syntactic sugar left to cover. _Prefixes_, _operators_, and square bracket structures from Basil's lexical structure are transformed into blocks during parsing. This allows us to write code with more expressive operators while still allowing us to reason about the code's structure in a simple way. Here are all such transformations in Basil:

| Syntax                | Equivalent                    | Description                                                               |
|---                    |---                            |---                                                                        |
| `A -> B`              | `(lambda! A B)`               | Defines an anonymous function.                                            |
| `A => B`              | `(metalambda! A B)`           | Defines an anonymous function that quotes its argument.                   |
| `A -< B`              | `(macro! A B)`                | Defines an anonymous macro.                                               |
| `A =< B`              | `(metamacro! A B)`            | Defines an anonymous macro that quotes its argument.                      |
| `A = B`               | `(set! A B)`                  | Assigns value `B` to `A`.                                                 |
| `A := B`              | `(define! A B)`               | Defines variable `A` equal to `B`.                                        |
| `-A`                  | `(0 - A)`                     | Negates value `A`.                                                        |
| `+A`                  | `(0 + A)`                     | Expresses positive `A` - does nothing other than integer promotion.       |
| `!A`                  | `(eval! A)`                   | Evaluates `A` in the current environment.                                 |
| `~A`                  | `(~ A)`                       | Creates a reference to `A`.                                               |
| `[A B C]`             | `(quote! (A B C))`            | Creates a quoted block of terms.                                          |

```rb
\ basil syntax postcard
\
\ all symbols other than those described above are 
\ user-defined variables or built-in functions

let dist(x, y) -> {
    x^2 := (n =< *: !n !n) x
    y^2 := (m -< -m * -m) y
    x^2 = x^2 + y^2; (=> sqrt +x^2)()
}
println:
    dist.[3 4]
```

## 3.2 - EBNF Grammar

```rb
Term        :  <integer constant>
            |  <rational constant>
            |  <symbol constant>
            |  <string constant>
            |  <character constant>
            |  <boolean constant>
            |  <identifier>
            |  "(" ")"
            |  "[" "]"
            |  "-" Term           
            |  "+" Term           
            |  "!" Term           
            |  "~" Term           
            |  Term "->" Phrase     
            |  Term "=>" Phrase     
            |  Term "-<" Phrase     
            |  Term "=<" Phrase     
            |  Phrase "=" Phrase      
            |  Phrase ":=" Phrase     
            |  "[" Term+ "]"      
            |  Block

Phrase      :  Term*

Line        :  (Phrase ";")* Term* 

Block       :  "(" (Line "\n")* Term* ")"
            |  "{" (Line "\n")* Term* "}"
            |  ":" Phrase
            |  ":" "\n" <indent> (Line "\n")* <dedent> (* 1 *)

Program     : (Line "\n")*
```

1. `<indent>` and `<dedent>` describe changes in indentation relative to the starting line. The block contains all lines after the `:` indented further right than the starting line until a line indented the same or further left than the starting line is encountered.

---

# 4 - Type System

Basil is a statically, strongly typed language. All values in a Basil program, at compile time and runtime, have types and obey typing rules. The type of a value defines not only the set of operations and behaviors it can be used for, but also the runtime representation of that value. 

Types may _convert_ to other types. There are two kinds of conversions - _implicit_ and _explicit_. There's no particular rule for whether or not a conversion has to be implicit or explicit - it varies type by type. But an implicit conversion generally represents an easier conversion than an explicit one. Implicit conversions are always a subset of explicit conversions - a type can always be explicitly converted to anything it can be implicitly converted to.

In this section, we'll discuss the different kinds of types Basil supports, how they are represented, and how they interact with each other.

## 4.1 - The Any Type

The _any_ type is a special type. It has no runtime representation or size, only existing as a compile-time wildcard for any type. Every other type can be implicitly converted to the any type, regardless of its other traits.

## 4.2 - Numeric Types

Numeric types in Basil are defined in terms of three facets:
 * Size - the size in bytes of the type.
 * Signedness - whether or not the type can represent negative values.
 * Floating - whether or not the type is stored using floating-point representation. All floating types are signed.

Other than to _any_, numeric types cannot convert implicitly or explicitly from any non-numeric type.

Within numeric types, implicit conversions depend on the three above facets. If a type is floating, it can be implicitly converted to to any other floating type with the same or greater size. If a type is not floating (or alternatively, _integral_), it can be implicitly converted to any other integral type with the same signedness.

Explicit conversions are more general. Numeric types can be explicitly converted to any other numeric type, with one exception: a floating type can only be explicitly converted to another floating type.

## 4.3 - Reference Types

A _reference_ type is a type that points to, or refers to, another value. Each reference type has a _target type_ - the specific type to which it points. Regardless of target type, all reference types have the same size - currently, eight bytes.

The target type of a reference type must be _mutable_. Mutability is a somewhat tangential part of Basil's type system. It is a characteristic of a value's type, not a type itself, and represents that the value can be modified after its initialization. Only very specific kinds of values have mutable types - this is discussed in section 7. Any value with a mutable type can be implicitly converted into a reference to its type. All references themselves are mutable.

Reference types cannot be implicitly or explicitly converted into other reference types, or any other type, with one exception: a reference type can be implicitly converted into its target type.

## 4.4 - Tuple Types

A _tuple_ type is a type that describes values (tuples) made up of one or more smaller values. Each of these smaller values is known as a _member_, and has a _member type_. The member types within a tuple type are _ordered_.

A tuple type's size is at least as large as the sum of the sizes of its members. Each member within a tuple must also be aligned to its type's size, potentially increasing the size of the tuple type beyond the initial sum. Members within a tuple must be ordered in memory in the same order as their corresponding member types.

A tuple type may be implicitly converted to another tuple type if, for each member type _A_ in the first tuple, the second tuple has a member type _B_ at the same ordinal that _A_ can implicitly convert to. Additionally, a tuple type can be implicitly converted to an array type, provided its member types are all implicitly convertible to the array's element type, and the tuple and array types have the same number of members.

Similarly, a tuple type may be explicitly converted to another tuple type if for each member type _A_, there exists a member type at the same ordinal in the second tuple that _A_ can explicitly convert to. And once again, a tuple type can be explicitly converted to an array type, provided its member types are all explicitly convertible to the array's element type, and the tuple and array types have the same number of members.

A tuple type may also be explicitly converted to the _type_ type, if all of its members are also of the _type_ type. This means that a tuple value containing only types can itself be used as a type.

## 4.5 - Array Types

An _array_ type can be thought of as a special case of the tuple type. The size and representation and nomenclature of an array type are all the same as a tuple. What makes an array type unique is that all of its member types must be identical - this type can be referred to as the _element type_ of the array. The number of members in an array is also known as its _length_.

An array type can be implicitly converted to any array type of the same length, provided its element type is implicitly convertible to the second's. An array type can also be implicitly converted to a tuple type, so long as the tuple type has a number of elements equal to the array's length, and only contains member types the array's element type can implicitly convert to.

An array type can be explicitly converted to any array type of the same length, provided its element type is explicitly convertible to the second's. An array type can also be explicitly converted to a tuple type, so long as the tuple type has a number of elements equal to the array's length, and only contains member types the array's element type can explicitly convert to.

## 4.6 - Union Types

A _union_ type has several member types, like a tuple type. However, it describes a value that contains only _one_ of its member types' values at a time. Unlike a tuple type, the members of a union type are not ordered. A union type permits duplicate member types. Any member types without duplicates are considered _unique_.

A union type's size is equal to the size of its largest member type, plus the size of the _type_ type. Its runtime representation is a value of the _type_ type, followed by a value of one of its member types.

Union types may implicitly convert to another union type with the same number of members, provided the second union's member types are either equal to the first's, or replaced with the _any_ type. They may explicitly convert to any of their unique member types. A union type can also explicitly convert to the _type_ type, if all of its member types are also the _type_ type.

## 4.7 - Intersection Types

An _intersection_ type has several member types, like a tuple type. It describes a single value that can act as any one of several member types. Unlike a tuple type, the members of an intersection type are not ordered. An intersection type permits duplicate member types. Any member types without duplicates are considered _unique_.

An intersection type's size depends on the specifics of its member types. In general, an intersection type's size is equal to the sum of its member types' sizes, possibly with some additional size for alignment. However, unlike a tuple type, values of an intersection type do not need to be ordered, and can be freely rearranged to minimize alignment overhead.

Intersection types may implicitly convert to another intersection type with the same number of members, provided the second intersection's members are either equal to the first's, or replaced with the _any_ type. They may explicitly convert to any of their unique member types. An intersection type can also explicitly convert to the _type_ type, if all of its member types are also the _type_ type.

## 4.8 - List Types

A _list_ type describes a cell in a singly-linked list. List types possess an _element type_, describing the type of value stored in each cell of the list.

The size of a list type value is equal to that of a reference type. Internally, a list value is a pointer to a value of its element type, followed by a value of the same list type which links to the next element.

List types may only implicitly or explicitly convert to a list type with element type _any_. No other conversions are permitted.

## 4.9 - Block Types

A _block_ type describes a quoted block of terms. The block type is special because it does not exist at runtime, representing source code for the purposes of introspection and metaprogramming. As such, there is no particular internal representation.

Block types have a sequence of any number of member types, similar to tuples, although unlike tupes a block type is permitted to contain no members. Like tuples, a block type may be implicitly converted to a block type of the same length, for which each member type in the first tuple can be implicitly converted to its corresponding member type in the second.

This applies to explicit conversions as well - again, like a tuple, a block type may be explicitly converted to a block type of the same length, for which each member type in the first tuple can be explicitly converted to its corresponding member in the second.

A block type may also be explicitly converted to the _type_ type, if and only if each of its member types is also of the _type_ type.

### Container Conversions

Finally, block types have a number of _special_ explicit conversions, in which they may be converted to a number of other compound types. In order to convert, the block _value_ must actually evaluate in the current environment, creating a number of values. Depending on the types of these values, and the desired type, a conversion may or may not be possible.

A block type may be explicitly converted to a tuple type if, upon evaluating the block, the number of values produced equals the number of members of the tuple. In addition, from first to last, each value must be explicitly convertible to the member type at the same ordinal in the tuple.

A block type may be explicitly converted to a list type if, upon evaluating the block, each value produced is explicitly convertible to the list's element type. A singly-linked list containing the values from first to last is produced. A block that produces no values will produce an _empty list_.

A block type may be explicitly converted to an array type if all of the values produced are explicitly convertible to the array's element type. In this case, an array containing each produced value is produced. If no values are produced, a compile error is printed.

## 4.10 - Function Types

A _function_ type represents a function, with an input and output. Specifically, a function type has an _argument_ type and a _return_ type. Additionally, a function type may possess one or more _constraints_, and may be _quoting_ or _non-quoting_.

A function type has the same size as a reference type.

Functions may be _constrained_ in one of several ways. These constraints may limit a function to accept only a particular value as input. Constraints may take _precedence_ over other constraints, or _conflict_ with other constraints. The constraints currently defined in Basil are as follows:
 * _unknown_ - The _unknown_ constraint represents a function which may or may not truly have constraints. It takes precedence over all other constraints, and conflicts with all other constraints.
 * _equals-value_ - The _equals-value_ constraint limits a function to accept only one value. This value must be determined at compile-time. It takes precedence over the _of-type_ constraint, and conflicts with other _equals-value_ constraints if they require the same value.
 * _of-type_ - The _of-type_ constraint limits a function to accept values that are explicitly convertible to its argument type. It conflicts with other _of-type_ constraints.

A function may possess any number of constraints - but no two constraints can conflict with one another.

A function type may implicitly convert to another function type if and only if:
 * The argument types of both functions are the same, or the target's argument type is _any_.
 * The return types of both functions are the same, or the target's return type is _any_.
 * Both functions are quoting, or both functions are non-quoting.
 * The second function has only one constraint - the _unknown_ constraint.

A function type may explicitly convert to the _type_ type if its argument and return types are both the _type_ type, and it possesses only one constraint - an _equals value_ constraint with a value of type _type_. This may seem strange, but like other conversions to the _type_ type, it allows us to represent function types using the same syntax as other functions. For example, `bool -> bool` may be converted to the _type_ type, representing a function type from `bool` to `bool`.

## 4.11 - Macro Types

A _macro_ type is very similar to a function type. Macro types describe classes of compile-time macros. Macro types, like function types, have an _argument_ type, one or more _constraints_, and may be _quoting_ or _non-quoting_. The primary difference between a macro type and a function type is that a macro type lacks a return type.

Macro types are compile-time only, and have no size or runtime representation.

Macro types may be constrained in an identical manner to function types, and obey identical implicit and explicit conversion rules - just minus any mention of the return type.

## 4.12 - Other Primitives

A _primitive_ type is a unique type built into Basil which does not fall into any other class of types. They have no special conversion rules with other types, and describe unique kinds of data.

The _boolean_ type represents a true or false value. It has a size of one byte.

The _string_ type represents a string of text. A string has a size equal to that of a _reference type_. Its representation is that of a pointer, pointing to a 4-byte unsigned _size_, followed by a 4-byte unsigned _capacity_, followed by a byte array with length equal to said capacity.

The _void_ type represents the absence of a value. It has a size of 0, and is only comprised of one value - the void constant `()`.

The _symbol_ type represents a unique name or identifier declared in the program. It has a size of 4 bytes.

Finally, the _type_ type represents a type itself. It has a size of 4 bytes at runtime.

---

# 5 - Evaluation

Syntactically, Basil is comprised of blocks of terms. This structure expresses the organization of Basil code. In this section, we'll discuss how terms are _evaluated_ to produce typed _values_, which better express the action and meaning of a Basil program.

## 5.1 - Values

A _value_ is any code entity that possesses a _type_. From a high level, values are the actual pieces of information that a program interacts with and operates on.

Basil terms may be evaluated to produce values. Evaluation occurs within a particular environment. What values a term produces depend on this environment - the number and types of the values may vary.

Basil terms also _are_ values. If evaluation does not occur (also referred to as _quoting_), a Basil term may be treated as a value in its own right. The type of this value depends only on the kind of term, not on any surrounding environment.

## 5.2 - Environments

Evaluation takes place within an environment. Specifically, an environment is made up of two components - a _stack_ and a _scope_. The _stack_ is a container of values. Values can be popped from the end of the stack, or pushed onto the end. The _scope_ is a collection of variables declared within the environment. Each of the variable entries in a scope is comprised of a _name_, associated with a type and optionally a value.

Environments themselves exist in a tree structure. Each program begins with a _root_ environment, and a _global_ environment descending from the root. The root is generally used for language-level variables, while the global environment is the base environment in which the program's evaluation takes place. From there, new environments can be introduced during evaluation, descending from the global environment or one of its descendants.

The scopes of environments in this tree structure are connected. Specifically, if an environment descends from another, we can describe the first environment's scope as a _child scope_ of the second, or the second environment's scope as a _parent scope_ of the first.

## 5.3 - Term Values

As described in 5.3, all terms have values, whether or not they are evaluated. This section describes the values associated with each term in Basil.

All constants other than the empty constant operate very similarly. Both before and after evaluation, constants have the same type and represent the same constant information. The only effect evaluation has is that the constant is no longer a term, and cannot be evaluated a second time. But quoted and evaluated constant terms can be used otherwise interchangeably to represent the same data.

| Constant              | Type                          |
|---                    |---                            |
| Integer               | Eight-byte, signed integer.   |
| Rational              | Eight-byte floating-point.    |
| Symbol                | _symbol_ type.                |
| String                | _string_ type.                |
| Character             | One-byte, unsigned integer.   |
| Boolean               | _boolean_ type.               |
| Void                  | _void_ type.                  |

The _empty_ constant is an exception. Empty constants have an empty block type before evaluation. Unlike any other constant, however, evaluating an empty constant will _not produce a value_.

Besides constants, we also have the _identifier_ term, also known as the _variable_ term. Prior to evaluation, variable terms have the _symbol_ type, and represent the symbol value corresponding to their name. When evaluated, a variable term will produce a _variable value_. The properties of this value depend on the surrounding environment(s). Starting from the current environment, the variable's name will be looked up in the environment's scope. If found, the variable value's type will be the associated type in that scope. Otherwise, the search is repeated with the current environment's parent, and so on until either the variable is found or the root scope is checked. It is a compile error to compute a variable's type if it doesn't exist in an enclosing scope. Additionally, variable values are _mutable_.

Finally, we have the block term. Prior to evaluation, block terms have a block type, with each term within the block as a member. For example, a block containing three variable terms will have a block type of three members, each member having type _symbol_.

Evaluating a block is more complex than for other terms, and is described in the next section.

## 5.4 - Evaluation Procedure

The evaluation of a Basil program is done wholly within blocks. Evaluating a block of terms involves the introduction of a new environment. Then, each term in the block is visited, from left-to-right. Each visited term goes through the following procedure in the block's environment:

1. If the top value on the stack has a _quoting_ function or macro type, push the term onto the stack without evaluating it.
2. If the top value on the stack has the _type_ type, and the term is a variable term, pop the top value from the stack and push a _definition_ from that value's type and the term's name.
3. Evaluate the term. Each value produced is pushed onto the stack.

Whenever a value is pushed to a stack, we perform the following checks:

1. If the top value on the stack can _interact_ with the pushed value, pop the top value, add the pushed value to the stack, re-add the popped top to the stack, and finally push the interaction.
2. If the pushed value can _interact_ with the top value on the stack, add the pushed value to the stack, push the interaction onto the stack.
3. If the top value on the stack _matches_ the pushed value, pop the top value and _apply_ the top value to our pushed value in the current environment.
4. If the pushed value _matches_ the top value on the stack, pop the top value and _apply_ the pushed value to the top value in the current environment.
5. Otherwise, add our value to the end of the stack.

When we are done evaluating a block, we are left with some number of values. Each value remaining, from the bottom to the top of the stack, is pushed to the enclosing block's stack.

Evaluation starts in the global environment. Every block in the program is evaluated, top-to-bottom, within a _global block_. When the global block is done evaluating, evaluation terminates.

The _result_ of evaluating a program is unspecified. This is because evaluation itself might have different meanings depending on the implementation. A Basil interpreter might directly execute the program as it evaluates, and print any remaining values at the end to a console. A Basil compiler, such as the reference implementation, might not execute any code at all, instead building an abstract syntax tree for the program to later convert into machine code.

## 5.5 - Matching

When a value is pushed to the stack, it is determined if it can _match_ with the top value, and if so, it may undergo _application_. 

Matching is the process of determining if a value is an appropriate argument for a function or macro. Specifically, three kinds of values can match, with the following logic:
 * A value of a function type can match with any value explicitly-convertible to its argument type.
 * A value of a macro type can match with any value matching its constraint. Unknown and of-type constraints will match any value explicitly-convertible to the macro's argument type. An equals-value constraint will only match a value equal to the constraint's desired value.
 * A value of an intersection type can match with a value if one of its members can match with the value.

Because function types and of-type constraints may permit multiple different types as inputs, it's possible that more than one value in an intersection can match a particular input. If this occurs, _overload resolution_ comes into play:
1. First, members that have an argument type _equal_ to the value's type take priority.
2. If no such members exist, members that have an argument type the value can _implicitly_ convert to take priority.
3. If no such members exist, members that have an argument type besides _any_ take priority.

Priority effectively eliminates all other members from consideration, if at least one member possesses that priority level. For example, if an intersection is in overload resolution, and has a member with an argument type _equal_ to the value's, no member without an equal argument type will be considered.

If after overload resolution there are multiple matching members of the same priority, a compile error is printed - this is considered an _ambiguous match_.

## 5.6 - Application

When a value matches another, application of the first value to the second occurs. Application takes two different forms depending on what value is being applied.

If a function is applied to a value, it is _called_ on that value. The result of invoking the function with that value as an argument is then pushed to the stack, a value with a type equal to the return type of the function.

One special case of the above behavior occurs if a function has the _any_ type as its argument. In this case, the function is _instantiated_ before it is _called_. Instead of calling the function itself, a new version of that function is created, with the matched value's type as its argument type instead of _any_. This new function is applied to the matched value.

If a macro is applied to a value, it is _expanded_ with that value. If the macro's environment contains any variable entries in its scope, the matched value is bound to that entry, and that entry is copied into the current evaluation environment. If this variable's name conflicts with a name in the enclosing scope, a new name without conflicts is chosen, replacing all occurrences of the old name in the macro body.

One special case of the above behavior occurs if a macro generates another macro in its expansion. When the inner macro is expanded, it must add both the argument variable of the outer macro _and_ its own argument variable to the current environment, if they exist. In this way, macros can be curried similarly to functions, to implement taking multiple arguments.

## 5.7 - Definitions

Variables can be _defined_ during normal evaluation procedures, if a variable term follows a type value.

Defining a variable modifies the _nearest declaration scope_. While every environment has a scope, definitions can only be added to the global scope, or to the scopes introduced by functions.

When a definition is created, it adds an entry to the nearest declaration scope. The name and type of this entry are based on the variable term and type value from which the definition originates.

It is a compile error if a variable is defined that already exists in the nearest declaration scope.

## 5.8 - Interactions

An _interaction_ is a special kind of procedure. Instead of being invoked through a value of a function or macro type, an interaction is automatically called whenever a value is pushed to the stack and a valid interaction procedure exists between the top value and pushed value.

Functionally, an interaction still behaves similarly to a variable. It has a type, and is associated with the scope of a particular environment. Unlike a variable, however, the name of an interaction's scope entry must _not_ be a valid identifier. This means that interactions cannot be explicitly referenced by name.

Interactions are associated with a pair of types. Interaction is not commutative - an interaction existing between types A and B does not imply an interaction exists between B and A.

Interactions will _match_ any pair of types that can be _explicitly converted_ to the original types they were declared between. If more than one interaction matches a pair of types, _interaction resolution_ occurs. This is equivalent to performing overload resolution (as described in 5.5 - types that can merely be _implicitly_ converted take precedence, and types that are _equal_ take precedence over those) on the first type, and if ambiguity persists, performing overload resolution on the second type. If more than one valid interaction still exists after performing overload resolution on each type, a compile error is printed.

---

# 6 - Built-Ins

Basil supports a number of built-in variables and procedures for common operations and tasks. These built-ins may represent or perform things impossible for user-defined code. But they still operate like normal variables, have types, and use normal matching logic.

## 6.1 - Root Scope Entries

All built-ins are defined in the scope of the root environment.

This includes the following built-in type variables:

| Aliases                   | Description                                                           |
|---                        |---                                                                    |
| `i8`, `byte`              | One-byte signed integer type.                                         |
| `i16`, `short`            | Two-byte signed integer type.                                         |
| `i32`, `int`              | Four-byte signed integer type.                                        |
| `i64`, `long`             | Eight-byte signed integer type.                                       |
| `u8`, `char`              | One-byte unsigned integer type.                                       |
| `u16`                     | Two-byte unsigned integer type.                                       |
| `u32`                     | Four-byte unsigned integer type.                                      |
| `u64`                     | Eight-byte unsigned integer type.                                     |
| `f32`, `float`            | Four-byte floating-point type.                                        |
| `f64`, `double`           | Eight-byte floating-point type.                                       |
| `type`                    | Represents the _type_ type.                                           |
| `string`                  | Represents the _string_ type.                                         |
| `symbol`                  | Represents the _symbol_ type.                                         |
| `void`                    | Represents the _void_ type.                                           |
| `bool`                    | Represents the _bool_ type.                                           |
| `any`                     | Represents the _any_ type.                                            |

And also the following built-in functions:
| Aliases                   | Description                                                           |
|---                        |---                                                                    |
| `+`                       | Adds numbers.                                                         |
| `-`                       | Subtracts numbers.                                                    |
| `*`                       | Multiplies numbers.                                                   |
| `/`                       | Divides numbers.                                                      |
| `%`                       | Finds remainder of division between two numbers.                      |
| `,`                       | Creates tuples.                                                       |
| `\|`                      | Creates unions.                                                       |
| `&`                       | Creates intersections.                                                |
| `~`                       | Creates references.                                                   |
| `<`                       | Less-than comparison.                                                 |
| `<=`                      | Less-than-or-equal-to comparison.                                     |
| `>`                       | Greater-than comparison.                                              |
| `>=`                      | Greather-than-or-equal-to comparison.                                 |
| `==`                      | Equality comparison.                                                  |
| `!=`                      | Inequality comparison.                                                |
| `and`                     | Logical and.                                                          |
| `or`                      | Logical or.                                                           |
| `xor`                     | Logical exclusive or.                                                 |
| `not`                     | Logical not.                                                          |
| `lambda!`, `lambda`, `λ`  | Anonymous function.                                                   |
| `macro!`, `macro`         | Anonymous macro.                                                      |
| `metalambda!`, `metalambda`   | Anonymous quoting function.                                       |
| `metamacro!`, `metamacro`     | Anonymous quoting macro.                                          |
| `set!`                    | Assigns value to another.                                             |
| `define!`, `define`, `let`| Type-inferring definition.                                            |
| `quote!`, `quote`         | Quotes term.                                                          |
| `eval!`, `eval`           | Evaluates quoted value.                                               |
| `print`                   | Prints value to standard output.                                      |
| `as`, `of`                | Casts value to target type.                                           |
| `list`                    | Creates list types.                                                   |
| `cons`, `::`              | Forms a linked list cell from two values.                             |
| `head`                    | Accesses the head of a linked list from a cell.                       |
| `tail`                    | Accesses the rest of a linked list from a cell.                       |
| `relate`                  | Defines an interaction between two types.                             |
| `atom`                    | Creates a single-value type.                                          |
| `is`                      | Checks type of value.                                                 |
| `match`                   | Combines multiple cases.                                              |

Built-in functions generated by special operators or prefixes are given an extra alias with a `!` at the end. These aliases are the forms operators will generate. This permits for the redefinition of words like `lambda`, `quote`, or `eval` while leaving built-in operators intact. The aliases may still be redefined or reassigned if modifying special operator behavior is intended.

## 6.2 - Arithmetic Operations

Basil includes five arithmetic operators as built-in functions: `+`, `-`, `*`, `/`, and `%`.

All of these operators have the same type. Arithmetic operators accept two arguments of any numeric type. Signed integers are promoted to `i64`, unsigned integers are promoted to `u64`, and floating-point values are promoted to `f64`. If the two arguments have different types, an appropriate peer type is returned - an `f64` with either of the other types returns an `f64`, and an `i64` with a `u64` returns an `i64`.

Unlike many other languages, this polymorphic behavior is encoded into the following type:

```rb
f64 -> f64 -> f64
& u64 -> (i64 -> i64; & u64 -> u64; & f64 -> f64)
& i64 -> (i64 -> i64; & u64 -> i64; & f64 -> f64)
```

Do note that because arithmetic operators are just like any other Basil function, they have no precedence. `1 + 2 * 3` yields 9. This also allows arithmetic operators to be partially applied.

### `+` _`lhs` `rhs`_

The `+` function adds two integer or floating-point numbers together.

### `-` _`lhs` `rhs`_

The `-` function subtracts its second argument from its first.

### `*` _`lhs` `rhs`_

The `*` function multiplies two numbers together.

### `/` _`lhs` `rhs`_

The `/` function divides its first argument by its second.

### `%` _`lhs` `rhs`_

The `%` function finds the remainder after dividing its first argument by its second. For integers, this is equivalent to taking the first argument modulo the second. For floating-point numbers, this is equivalent to the amount remaining after subtracting the closest integer multiple of the second argument in the negative direction from the first.

```rb
\ example arithmetic expressions

1 + 2           \ 3
5.4 - 2.3       \ 3.1
4 * 4           \ 16
9 / 3           \ 3
5.6 % 3         \ 2.6
```

## 6.3 - Logical Operators

Basil defines four logical operators as built-in functions: the binary `and`, `or`, and `xor`; and the unary `not`.

Each of the binary operators takes two arguments of type `bool` and returns `bool`. The unary `not` takes one argument of type `bool` and returns `bool`. These types are expressed with the two type expressions below:

```rb
\ binary
bool -> bool -> bool

\ unary
bool -> bool
```

Like arithmetic operators, logical operators have no precedence, and can be partially applied.

### `and` _`lhs` `rhs`_

The `and` function returns the result of a logical AND between its two arguments.

### `or` _`lhs` `rhs`_

The `or` function returns the result of a logical OR between its two arguments.

### `xor` _`lhs` `rhs`_

The `xor` function returns the result of a logical XOR between its two arguments.

### `not` _`value`_

The `not` function returns the logical negation of its argument.

```rb
\ example logical expressions

true and false      \ false
true or false       \ true
true xor false      \ true
not true            \ false
```

## 6.4 - Comparison Operators

Basil defines two equality operators and four relational operators as built-in functions - `==` and `!=`, and `<`, `<=`, `>`, `>=` respectively.

Equality operators may accept any two numeric arguments and return a bool, and will perform numeric type promotion and resolution identical to that of arithmetic operators. Equality operators may additionally compare two arguments of type `bool`, `string`, `type`, or `symbol`.

Relational operators are more limited. They may accept two numeric arguments and return a bool, with numeric type promotion and resolution, identically to equality operators. In addition to numeric types, they can also compare two arguments of type `string`.

These types are expressed in the following two type expressions:

```rb
\ equality

f64 -> f64 -> bool
& i64 -> (i64 -> bool; & u64 -> bool; & f64 -> bool)
& u64 -> (u64 -> bool; & i64 -> bool; & f64 -> bool)
& bool -> bool -> bool
& string -> string -> bool
& type -> type -> bool
& symbol -> symbol -> bool

\ relational

f64 -> f64 -> bool
& i64 -> (i64 -> bool; & u64 -> bool; & f64 -> bool)
& u64 -> (u64 -> bool; & i64 -> bool; & f64 -> bool)
& string -> string -> bool
```

### `==` _`lhs` `rhs`_

The `==` function tests for equality between two values. It will return true if and only if:
 * Comparing two numbers, when the numbers share the exact same integer or floating-point value.
 * Comparing two booleans, when both booleans share the same value.
 * Comparing two strings, when the contents of both strings are identical.
 * Comparing two of the same type.
 * Comparing two of the same symbol.

### `!=` _`lhs` `rhs`_

The `!=` function tests for inequality between two values. It is equivalent to the logical negation of `==` between its arguments - `A != B` is true if and only if `A == B` is false, and vice versa.

### `<` _`lhs` `rhs`_

The `<` function tests if its first argument is less than its second. 

For numbers, this occurs when the first argument has a more negative value than the second. 

For strings, this occurs when the first argument occurs lexicographically before the second.

### `<=` _`lhs` `rhs`_

The `<=` function returns true if and only if its first argument is less than its second (via `<`) or equal to its second (via `==`).

### `>` _`lhs` `rhs`_

The `>` function tests if its first argument is greater than its second.

For numbers, this occurs when the first argument has a more positive value than the second.

For strings, this occurs when the first argument occurs lexicographically after the second.

### `>=` _`lhs` `rhs`_

The `>=` function returns true if and only if its first argument is greater than its second (via `>`) or equal to its second (via `==`).

```rb
\ examples of comparison expressions

1 == 1          \ true
string != i64   \ true
"abc" > "def"   \ false
"cat" <= "bat"  \ false
```

## 6.5 - The Join (Tuple) Operator

### `,` _`lhs` `rhs`_

The `,` function will join two values into a tuple. It accepts two arguments, which can be of any type, and will return a tuple of the first argument and second argument in that order. The `,` function has a type signature of `any -> any -> any`.

```rb
\ examples of tuple expressions

1, 2, 3         \ (1, 2), 3
1, (2, 3)       \ 1, (2, 3)
i64, i64        \ i64, i64 (can be used as type)
```

## 6.6 - The Intersection Operator

### `&` _`lhs` `rhs`_

The `&` function will form an intersection of two values. It accepts two arguments, which can be of any type, and will return an intersection of the two arguments. The `&` function has a type signature of `any -> any -> any`.

The `&` function has special behavior when called with functions or macros. If an intersection is made of two function or macro values with types that only differ in their constraints, the intersection will attempt to unify the two. This is only possible if the two functions' or macros' constraints do not conflict with each other. In the case this occurs with functions, the resulting value will have a function type, with the same argument and return types as its arguments and a union of its arguments' constraints. If this occurs with macros, the resulting value will likewise have a macro type, with the same argument type as its arguments and a union of its arguments' constraints.

```rb
\ examples of intersections

1 & true
(i64 x) -> x; & (f64 y) -> y
(i64 x) -> x; & 0 -> 0; & 1 -> 1
``` 

## 6.7 - The Union Operator

### `|` _`lhs` `rhs`_

The `|` function will form a union type. It accepts two arguments of type `type`, and will return a union type with those types as members. The `|` function has a type signature of `type -> type -> type`.

Unlike the `,` and `&`, `|` notably does not involve values, only types. This is because the union can only store one value at a time, but there is still a necessity to specify a number of different types the union may contain. `|` should be used to define a union type, and then a variable assignment or explicit conversion should be used to create a value of that union type.

```rb
\ examples of unions

i64 | bool
string | f64 | bool
```

## 6.8 - The Reference Operator

### `~` _`value`_

The `~` function will form a reference to a value. It accepts one argument of any type, provided the argument is _mutable_, and will return a reference to that value. The `~` function has a type signature of `any -> any`.

```rb
\ examples of references
x := 1
y := ~x
y = 2       \ will modify x
```

## 6.9 - Lambdas and Metalambdas

### `lambda` _`arg` `body`_

The `lambda` function (also `lambda!`, `λ`, and the `->` operator) is used to define anonymous functions. `lambda` itself is a function that takes two arguments, a _match_ value and a _body_. Both of these are automatically quoted when `lambda` is called, resulting in `lambda`'s type signature of `any => any => any`.

When both arguments are provided, `lambda` will attempt to declare a function from those two arguments. First, `lambda` defines three new environments, descending from the environment in which `lambda` was called:
 * `self`, a valid declaration scope, used to record a variable of the function itself and enable recursion.
 * `scope`, descending from `self`. This is a valid _declaration scope_, and is where the function's arguments and any variables in its body will be defined.
 * `body`, descending from `scope`. This is the environment in which the function's body will be evaluated.

Next, `lambda` defines its argument, which can be one of three things:
 * If its argument evaluates to a _definition_, the definition is evaluated within `scope` and defines a variable for the argument in it. The function's type will use the definition's type as its argument type, and have the of-type constraint.
 * If its argument evaluates to a _variable_ value, a variable is defined with the argument's name within `scope` with type `any`. The function's type will use `any` as its argument type, and have the of-type constraint.
 * If its argument can be evaluated at compile-time, no definition is created within `scope`. The function's argument type is equal to `lambda`'s first argument's type, and it will have the equals-value constraint with this argument's compile-time value.

Finally, `lambda` will attempt to evaluate its body within the `body` environment. If an error occurs, the function is considered incomplete, and has return type `any`. If no error occurs, the function's return type is equal to the type of the top value of the `body` environment's stack.

An incomplete function is not an error. Depending on the type of the function, it will be fixed later when it is assigned to a name or instantiated.

### `metalambda` _`arg` `body`_

The `metalambda` function (also `metalambda!`, and the `=>` operator) is almost identical to the `lambda` function, with only one difference: functions produced using `metalambda` will be _quoting_, while functions produced using `lambda` will not. `metalambda` itself still has a type signature of `any => any => any`.

```rb
\ example functions

adder := x -> y -> x + y
adder 1 2       \ 3

is-x := x => x == #x
is-x x              \ true
is-x y              \ false
```

## 6.10 - Macros and Metamacros

### `macro` _`arg` `body`_

The `macro` function (also `macro!`, and the `-<` operator) is used to define anonymous macros. `macro` takes two arguments, a _match_ value and a _body_. Both are automatically quoted when `macro` is called, resulting in `macro`'s type signature of `any => any => any`.

When both arguments are provided, `macro` will attempt to declare a macro from those two arguments. First, `macro` defines a new environment, `scope`, where its argument will be defined.

Next, `macro` defines its argument, which can be one of three things:
 * If its argument evaluates to a _definition_, the definition is evaluated within `scope` and defines a variable for the argument in it. The macro's argument type will equal the definition's type, and the macro will have the of-type constraint.
 * If its argument evaluates to a _variable_ value, a variable is defined with the argument's name within `scope` with type `any`. The macro's argument type will be equal to `any`, and the macro will have the of-type constraint.
 * If its argument can be evaluated at compile-time, no definition is created within `scope`. The macro's argument type is equal to `macro`'s first argument's type, and it will have the equals-value constraint with this argument's compile-time value.

Unlike `lambda`, macro will not attempt to evaluate its body eagerly. It will wait until it is expanded later.

### `metamacro` _`arg` `body`_

The `metamacro` function (also `metamacro!`, and the `=<` operator) is to `macro` what `metalambda` is to `lambda`. It is essentially the same, only the resulting macro will be _quoting_, and will not evaluate its argument.

```rb
\ example macros

dup := x -< x x
dup 2 *             \ 4

twice := body =< !body; !body
x := 10
twice(x = x + 1)    \ x = 12
```

## 6.11 - Assignment

### `set!` _`dest` `source`_

The `set!` function (also the `=` operator) will assign a value to another value, including any built-in behavior attached to that value. It takes two arguments of any type, but the first argument must be mutable. It does not return a value, and its type signature is `any -> any -> void`.

Assignments permit a special transformation if their first argument is a partially-applied _inferring definition_. Once both arguments are provided, if this is the case, the assignment value must replace itself on the stack with the inferring definition, using the assignment's second argument as the definition's initializer. This permits for both `define x 1` and `let x = 1` to be used when defining variables. This behavior _also_ occurs if the first argument is a partially-applied `relate`.

```rb
\ example assignments

x := 1
x = 2                               \ value is now 2
(bool -> bool -> bool) logic = and  \ value is now and
true logic true                     \ true
```

## 6.12 - Inferring Definitions

### `define` _`name` `initializer`_

The `define` function (also `define!` and `let`) will create a definition in the current environment, and assign the resulting variable to an _initializer_ value. Unlike another definition, however, the type of the variable is inferred from the initializer - specifically, the type of the value will equal the type of the initializer.

Because `define` does not specify the type of the variable _until_ the initializer is provided, binding a recursive function to the variable may initially result in an incomplete function, as the type of the recursive function depends on itself circularly. Once the initializer has been fully evaluated and the variable bound to the resulting value's type, the initializer will be _completed_ - any incomplete functions within the initializer will have their bodies evaluated, with the new variable entered into their `self` scopes. If evaluation still fails, a compile error is printed, unless the recursive function has argument type `any` in which case completion is deferred until the function is _instantiated_.

Assigning the initializer to the variable must associate the variable with any built-in behavior belonging to the initializer.

```rb
\ example definitions

let x = 2       \ x is i64
let y = 3       \ y is i64
let z = x + y   \ z is i64

define var define
var w = 10
```

## 6.13 - Quotes

### `quote` _`term`_

The `quote` function (also `quote!`) will return the quoted value of a term. It accepts one automatically-quoted argument of any type, and has type signature `any => any`.

`quote` can actually be defined almost trivially as a quoting identity function `x => x`. It exists mostly for convenience and to simplify generated code from bracketed block (`[ ... ]`) syntax.

```rb
\ example quote use

let expr = quote(1 + 2)
let get-3 = () -> !expr
x := get-3()      \ x = 3
```

## 6.14 - Explicit Evaluation

### `eval` _`quoted-term`_

The `eval` macro (also `eval!`, and the `!` operator) is a means of explicitly evaluating a quoted term. It accepts one argument of any type, and has type signature `any -<`. Unlike most other built-in procedures, `eval` is a macro, not a function, because evaluating an arbitrary term may yield any number of values.

`eval` performs evaluation using the normal evaluation procedure, and acts within the current environment.

```rb
\ example eval use

x := ![1 + 2]   \ x = 3
inc := [+ 1]  
1 !inc     \ 2
```

## 6.15 - Casts

### `as` _`value` `type`_

The `as` function will explicitly attempt to convert a value to a particular type. It also supports a number of other special conversions not usually supported by type conversion rules, only possible through the use of this function. `as` accepts two arguments, one of any type, and a second of type `type`.

`as` will attempt to convert its first argument to the type described in its second. It may perform any implicit or explicit conversion in doing so. 

Additionally, `as` can convert the following types beyond their normal conversion rules:
 * `as` can convert a floating-point type to an integral type. It will round towards zero in doing so.
 * `as` can convert a union type to one of its member types.
 * Likewise, `as` can convert an intersection type to one of its member types.

These additional cases are described using different function types. The type of `as` can be written as:

```rb
f64 -> type -> any
& (any & any) -> type -> any
& (any | any) -> type -> any
& any -> type -> any
```

### `of` _`type` `value`_

The `of` function is _almost_ an alias for `as`. It has the same behavior, with one caveat - its first and second arguments are swapped.

```rb
\ example explicit casts

x := 10 as i8           \ x is i8, x = 10

type variant = i64 | bool
b := variant of true    \ b is (i64 | bool), b = true
i := variant of 1       \ i is (i64 | bool), i = 1

bool_part = b as bool   \ bool_part = true
```

## 6.16 - Output

### `print` _`value`_

The `print` function will print a value to standard output. It can print either a numeric value or a string, and has a type signature of:

```rb
i64 -> void
& u64 -> void
& f64 -> void
& string -> void
```

### `println` _`value`_

The `println` function has the same type as `print`, and behaves almost the same, with the exception that an additional newline is printed after each value. `println` can also be called with a `void` argument to print an empty blank line.

```rb
\ example output operations

println "Hello world!"
print "1 + 2 is "; println (1 + 2)
```

## 6.17 - List Operations

### `list` _`type`_

The `list` function is used to refer to a list type. `list` takes one argument of type `type`, and returns a list type with the argument type as its element. `list` has a signature of `type -> type`.

### `::` _`head` `tail`_

The `::` function (also `cons`) creates a singly-linked-list cell from a value and a list. `::` takes a value of any type, and then a list with an element type equal to the first argument's type. This is implemented by giving `::` the type signature `any -> any list -> any list`.

### `head` _`list`_

The `head` function retrieves the head of a singly-linked list. It accepts a list of any type, and returns the value of its first cell. `head` has signature `any list -> any`. It is a runtime error to call `head` on an empty list.

### `tail` _`list`_

The `tail` function retrieves the tail of a singly-linked list. It accepts a list of any type, and returns the tail of its first cell. `tail` has signature `any list -> any list`. It is a runtime error to call `tail` on an empty list.

```rb
\ example list operations

int list nums = 2 :: [] \ nums = [2]
nums = 1 :: nums        \ nums = [1 2]
nums = 4 :: [3 2 1]     \ nums = [4 3 2 1]

nums head               \ 4
nums tail               \ [3 2 1]
```

## 6.18 - Defining Interactions

### `relate` _`(type1, type2)` `interaction`_

The `relate` function defines an interaction between two types. It behaves similarly to an inferring definition, only without a name - instead, `relate` first takes a `(type, type)` argument, and then any value, yielding the type signature `(type, type) -> any -> void`.

When both arguments have been provided, `relate` will create an interaction in the nearest declaration scope between the two types in its first argument. This interaction will be assigned to the second argument value - it takes on its type, and any associated built-in behavior.

```rb
\ example interaction usage

relate(i64, string) = (i64 x) -> (string op) -> op match {
    "++" -> x + 1
    "--" -> x - 1
}
1 "++"      \ 2
```

## 6.19 - Atoms

### `atom` _`name`_

The `atom` function is used to define a new type with only one value. It accepts one automatically-quoted argument of type `symbol`, and returns the type that it created. Upon receiving an argument, it will also define a variable in the current scope, storing the singleton value of the newly created type, named from the provided symbol.

The type created may implicitly convert to the _type_ type and the _any_ type. If converted to a _type_, the resulting type value will be the created type. This means the value defined by `atom` can also be used to access the type created by `atom`.

```rb
\ example atom types

atom RED; atom GREEN; atom BLUE
type color = RED | GREEN | BLUE     \ use of atom type

color c = color of RED              \ use of singleton value
c is BLUE                           \ false
```

## 6.20 - Is

### `is` _`value` `type`_

The `is` function checks whether a value has a particular type. It accepts an argument of any type, and then a second argument of type `type`. It returns a boolean, true if the value is of the given type, false otherwise. This form of `is` is resolvable entirely at compile-time. `is` has a type signature of `any -> type -> bool`.

A specialization of `is` exists for union values. `is` may return true if the value is of the given type, _or_ if the current value stored in the union is of the given type. The latter form may occur at runtime, but only applies if the type provided to `is` is a member type of the provided union. In all other cases, `is` is still resolvable entirely at compile-time.

```rb
\ example is usage

1 is i64            \ true

type opt-int = atom NONE | i64

x := opt-int of 1
x is opt-int        \ true
x is i64            \ true
x is NONE           \ false
```

## 6.21 - Match

### `match` _`cases`_

The `match` function allows the matching of an argument to a number of different cases. `match` takes one quoted argument, a block of _cases_, and returns a resulting function. `match` has a type signature of `any block -> any`.

Upon receiving its argument, `match` will evaluate it, and attempt to form an intersection between the values produced. If the intersection fails, or the resulting intersection contains any non-function or non-macro type, a compile error is printed.

```rb
\ example match

let f = match (1 -> "one"; x -> "many")
f 1                 \ "one"
f 2                 \ "many"

2 match {           \ 3
    2 -> 3
}
```

## 6.22 - Built-In Interactions

Basil supports a number of built-in interactions in addition to its other built-in procedures. These are more limited in scope as interactions are less explicit. But, they generally help allow for the "faking" of familiar syntax within Basil's evaluation model.

### _`type` `[any]`_

If a `type` interacts with a block of one element, it will attempt to create an array type value. The block is first evaluated. If only one value is produced, it has integral type, and has a value known at compile-time, an array type is returned with the produced value as its length. Otherwise, a compile error is printed. The signature of this interaction is `type -> [any] -> type`

### _`any array` `[any]`_

If any array interacts with a block of one element, it will attempt to access an element of that array at a particular index. Like the previous interaction, the block is evaluated to see if a single, integral value is produced. If so, that value is used as an index to randomly-access the array and return an element. Otherwise, a compile error is printed. The signature of this interaction is `any array -> [any] -> any`. The result of this interaction is _mutable_.

### _`any tuple`   `[any]`_

If any tuple interacts with a block of one element, it will attempt to access an element of the tuple at a particular index. This functions identically to the array indexing interaction, only instead of accessing an element of an array type, a member of the tuple is returned. The type of this member depends on which member is accessed, and as a result, this operation must be entirely resolvable at compile-time. The signature of this interaction is `any tuple -> [any] -> any`. The result of this interaction is _mutable_.

```rb
\ example container interactions

i64[3] triplet = [0 0 0]    \ array of three integers
triplet[0] = 1              \ index
triplet[1]                  \ 0

(i64, i64, i64) tup = triplet
tup[0]                      \ 1
tup[1] = 3
```

# 7 - Code Examples

This section serves as a demonstration of idiomatic Basil code. It does not introduce or specify any language features, and exists only to supplement the rest of the specification with some practical examples.

## 7.1 - Factorial

```rb
let factorial = match {
    0 -> 1
    i64 n -> n - 1 factorial * n
}

5 factorial        \ factorial(5) = 120
```

## 7.2 - Reverse Singly-Linked List

```rb
let reverse orig -> memo -> orig match {
    [] -> memo
    list -> reverse(list tail)(list head :: memo)
}

int list nums = [1 2 3]
reverse nums []     \ [3 2 1]
```

_to be expanded..._