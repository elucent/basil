# Basil - Quick Tour

Basil is a statically-typed multi-paradigm language influenced by Scheme, C++, Forth, and a host of others. The language's goal is, via permissive grammar and generic features, to support high levels of customization and expressiveness. Here's a quick tour of Basil's fundamental features.

## 1 - Structure

Basil's grammar is similar to most Lisp dialects - it is comprised of lists of lists.

```py
# Valid basil code
(define x 1)
(print (+ x 2))
```

Unlike most Lisps, however, a large number of equivalent syntaxes are available to reduce parenthese-clutter:
```py
# The following are all equivalent means of grouping A (B C)
A {B C}
A: B C
A B.C
A:
    B C
A; B C
```

Additionally, while most terms within lists are delimited by whitespace or list delimiters (e.g. parentheses), a few other terms are considered delimiters for more natural-looking code, such as the comma operator `,`.

This allows for more "familiar" code coming from the C++, Python, or ML world - while still retaining the "list of lists" structure:
```rb
# The following are equivalent:
(define plus-one 
    (i64 n) ->
        (+ n 1))

define plus-one(i64 n) -> {
    + n 1
}
```

This might look a bit odd. Think of it less as supporting multiple languages' syntaxes in one, more as having a lot of options when you want to write terse or expressive code.

## 2 - Types

Basil is a statically and strongly typed language. A quick tour of the type system:

 * Integer and floating-point types are included as primitives, with a number of different sizes. There are also boolean, character, and string types.
    ```py
    i8; i16; i32; i64   # Signed integer types
    u8; u16; u32; u64   # Unsigned integer types
    f32; f64            # Single and double precision floats
    bool                # Boolean type
    char                # UTF-8 char type
    string              # UTF-8 string type
    ```
 * Compound types (tuples, unions, and intersections) can be constructed with simple operators.
    ```py
    f64, f64            # Pair of doubles
    i64 | bool          # Union of i64 and bool
    i64 & bool          # Intersection of i64 and bool
 * Function types can be constructed from an argument type and a return type. Function types may additionally have _constraints_, expressed with compile-time expressions, which limit their arguments to those that match their constraints.
    ```rb
    i64 -> i64          # Function of i64 to i64
    1 -> i64            # Function of i64 to i64, only accepts 1

    (i64 -> i64) &      # Overloads can be combined
    (f64 -> f64)        # using intersection

    (0 -> i64) &        # Cases can be combined
    (1 -> i64)          # using intersection
    ```
 * Finally, there are some built-in types for compile-time values.
    ```rb
    type                # Type for a type
    meta                # Type for quoted code

## 3 - Stack-Based Evaluation

While Lisps generally use prefix notation to avoid the hidden structural complexity introduced by operator precedence parsing, Basil uses a stack-based model. It's definitely a bit more complex, but it allows for more syntax options, and works quite nicely with Basil's type system.

Basil is eager-evaluated. The program will evaluate lists of terms left-to-right. If a term is itself a list, it will be fully evaluated before returning values into the enclosing list.

This process uses a stack. Every time a new term is evaluated, it's pushed onto the stack. It then has a few chances to interact with the top item of the stack before it is evaluated and its value is pushed on the stack. The following rules define behavior when a term is pushed:
 
Before evaluating the term:
 * If the top item is a function with argument type `meta`: quote the term, apply the function to the quote, and push the result.
 * If the top item is a type, and the new value is a variable name, define a new variable with that name and type and push the definition.

After evaluating the term:
 * If the top item is a function with a compatible argument type to the term's value: pop the function, apply the function to the value, and push the result.
 * If the term's value is a function with a compatible argument type to the top item:
 pop the top item, apply the term's value to the top item, and push the result.
 * Otherwise, if no application occurred, push the term's value onto the stack.

In short, as we evaluate more and more of the program, each value has a chance to interact with the top item in both directions. Whether we use prefix (push the function first, then push arguments) or postfix (push the arguments first, then push function) notation, the evaluator will match the function with any available arguments and yield similar structures.
```rb
# The following yield identical syntax trees
+ 1 2 + 3
3 2 1 + +
1 + 2 + 3
```
Similar to Basil's grammar, this may seem a bit odd. Writing `3 2 1 + +` is a bit of a weird way to add three numbers. Once again, it's not the intent that these are alternative syntaxes, rather that options are available in the case that a particular ordering is more expressive or natural for a particular case.
```rb
print "Hello world!"    # Prefix notation seems natural
40 factorial            # Postfix notation seems natural
```

## 4 - First-Order Functions

All functions in Basil are first-order. Because functions are such a significant part of Basil's semantics, they are given unique syntax: the right-associative `->` operator.

Functions in Basil have exactly one argument. Functions with multiple inputs must accept them through currying. There's some syntactic sugar supported by the `->` operator to make declaring these functions a bit easier, though.
```rb 
# The following are equivalent
(i64 x) -> (i64 y) -> x + y
(i64 x)(i64 y) -> x + y
(i64 x; i64 y) -> x + y
```

Most built-in operators are functions as well, and can be partially applied.
```rb
define plus-one = + 1
```

## 5 - Metaprogramming

Functions may accept as arguments or return compile-time types such as `meta` or `type`. For types, this lets us write natural code to accomplish what template or generic syntax would accomplish in other languages.
```rb
define pair = (type a; type b) -> a, b
pair(i64; i64) p = 1, 2
```

With `meta`, we can define functions that look like custom syntax, specifying when and how evaluation should occur. The built-in function `eval` is used to expand a block of code referred to via a `meta` object.
```rb
define if = (meta cond; meta body) -> {
    define cases = (true -> eval body) & (false ->)
    cases(eval cond)
}

if (1 < 2): print "Hello world!"
```

The caveat with these functions is their compile-time nature. Types and code may be manipulated using a variety of operations, but these operations must terminate and no compile-time types must remain by the time the program is compiled.

## 6 - Built-In Functions

Basil includes a number of functions built into the compiler by default, implemented by the compiler itself instead of by some Basil code.

Arithmetic operations are built-in. `+`, `-`, `*`, `/`, and `%` are defined, and each has overloads to support operations with floating-point and integer numbers. Integer division truncates towards zero. Operations can be done between different types, and will yield the more general type.
```rb
1 + 2 * 3 - 4   # Result is i64
10 + 20.0       # Result is f64
```

Relational and comparison operators are built-in. `==`, `!=`, `<`, `>`, `<=`, and `>=` are defined. Every operator returns `bool`, and accept any numeric types as arguments. `==` and `!=` can additionally compare two `bool`s for equality.

Logical operators are built-in. Binary operators `and`, `or`, and `xor`, as well as unary operator `not`. Each takes only `bool` for arguments, and returns `bool`.
```
1 == 2 and not 3 < 4
```

Type operators are built-in. This includes `,`, `|`, and `&` for tuples, unions, and intersections respectively. Using them will create compound objects from their arguments. If their arguments are both of type `type`, then the result can also be cast to `type`.
```rb
i64, i64 x = 1, 2
```

Definitions and assignments are represented with built-in functions. Assignments are done with `=`, and are actually a special case in the grammar - `=` is right-associative. Definitions are done using the `define` built-in function, which will infer the variable's type.
```rb
define x 1
x = 2
define y = 2    # = is permitted here stylistically. 
                # This is identical to define y 2
y = x + 1
```

Finally, a few common syscalls are represented as built-in functions. Currently just `print` is implemented, but `exit`, file operations and heap allocation are on the to-do list.
```rb
print "I'm a print statement!"
```

---

That's a somewhat brief introduction to the language! Hopefully you found it interesting. :)