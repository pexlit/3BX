# 3BX Language Specification

## Overview

3BX is a language designed to help humans understand code better and agents code better.

Almost all syntax is defined in the language itself. The compiler should focus on context.

3BX is not aimed to 'satisfy the compiler'. Still, the compiler has to figure out every little detail, like variables and the type of each variable.

3BX compiles to machine code using LLVM, optimizing for the highest performance possible.

## Syntax Basics

### Indentation

Indents are done with either tabs or spaces. A source file has either a fixed amount of tabs or spaces per indent.

### Special Characters

Code should use as few special characters as possible (so custom patterns can use them). Reserved characters:

- `:` indicates a section
- `#` indicates a comment
- `"` starts and ends strings

## Sections

Sections define and use patterns. There are two categories:

### Definition Sections

These add new syntax to the language:

- `expression:` can be set, get, added to, etc.
- `event:` defines an event which will trigger all event listeners when triggered
- `effect:` executes code

### Usage Sections

These use existing syntax but do not add new patterns:

- `condition:` when the expression before the `:` evaluates to true, the section will be executed
- `loop:` the section will be executed multiple times depending on what the loop does

## Patterns

### Pattern Syntax

Patterns are built up using required and optional segments. Below are some examples of combinations a pattern can evaluate to:

```
print [a|the] message:
```

This matches:
- `print message`
- `print a message`
- `print the message`

### Pattern Tree Optimization

The pattern tree should join branches after optional segments to avoid having 200 branches for one pattern with 8 optional segments. When another pattern also uses the same branch, it should split them. Otherwise issues like these occur:

```
give [some] bread to dave
give some bread to jane  # no optional segment! but since the tree wired
                         # the end of [some] back to the end of give,
                         # "give bread to jane" will compile incorrectly
```

In that case, split the branches: duplicate `bread to dave` to the `some` branch.

### Returning Values

To return a value in an expression, use either `set result to x` or `return x`.

## Variable Deduction

Variables don't need special characters for indication. Instead, variables are deduced as follows:

### Step 1: Intrinsic Analysis

Variables are deduced from `@intrinsic` calls. The arguments in those calls are guaranteed to be variables.

```
effect set var to val:
    when triggered:
        @intrinsic("store", var, val)  # var and val are variables!
```

### Step 2: Pattern Incorporation

Incorporate those variables into the patterns of the expressions and effects they are in:

```
effect set $ to $
```

### Step 3: Propagation

Do the same with patterns that have deduced variables, and repeat step 2 until every variable has been resolved.

```
effect test var:
    when triggered:
        set var to 1  # since we know 'set var to 1' uses the pattern
                      # 'set $ to $', we can deduce 'var' and '1' as
                      # arguments. Therefore 'var' is a variable!
                      # This effect's deduced pattern becomes 'test $'
```

## Compiler Design

The compiler has two jobs:

1. Tokenize structure (indentation, strings, numbers, words, punctuation)
2. Match tokens against patterns defined in .3bx files

### Lexer

The lexer only handles structural elements:

- `INDENT` / `DEDENT` - indentation changes
- `NEWLINE` - line endings
- `STRING` - quoted text
- `NUMBER` - numeric literals
- `IDENTIFIER` - any word (no exceptions)
- Punctuation - `: , ( ) [ ]` etc.

The lexer does NOT categorize words. `set`, `if`, `while`, `function` are all just IDENTIFIERs. The lexer has no concept of keywords.

### Parser

The parser matches token sequences against registered patterns. It does not have hardcoded grammar rules.

When parsing a line like `set x to 5`:

1. The lexer produces: `IDENTIFIER("set") IDENTIFIER("x") IDENTIFIER("to") NUMBER(5)`
2. The parser tries to match this against all registered patterns
3. The pattern `set $ to $` from prelude.3bx matches
4. `set` and `to` are matched as literals because they appear literally in the pattern
5. `x` and `5` are captured as variables because they match `$` positions

### Anti-Patterns

The compiler must NOT:

- Have hardcoded keywords in the lexer
- Have reserved word lists anywhere
- Have special token types for `if`, `set`, `while`, etc.
- Have hardcoded grammar rules for control flow or declarations
- Have expression precedence rules in the parser (precedence comes from patterns)

If you find yourself adding a keyword check or special case for a word, you are doing it wrong. Define a pattern in prelude.3bx instead.

### Design Philosophy

Traditional compilers bake syntax into the compiler. 3BX bakes syntax into the language. This means:

- Users can define new syntax without modifying the compiler
- The compiler is simpler (just pattern matching)
- The language is self-documenting (read prelude.3bx to understand syntax)

## Testing

To see how well the compiler is doing, use `run_tests.sh`. For now it isn't bad if the tests fail, but implement the missing features one by one.

DO NOT modify any tests in the `tests/required` folder. You may modify all files in the `tests/agents` folder to test out things.
