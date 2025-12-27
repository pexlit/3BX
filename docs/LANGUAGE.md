# 3BX Language Reference

## Table of Contents

1. [Language Overview](#language-overview)
2. [Basic Syntax](#basic-syntax)
3. [Pattern System](#pattern-system)
4. [Intrinsics](#intrinsics)
5. [Standard Library](#standard-library)
6. [Examples](#examples)

---

## Language Overview

3BX is a programming language designed to be readable by both humans and AI. It features:

- **Natural Language Syntax**: Inspired by Skript, 3BX uses human-readable patterns like `set x to 42` instead of cryptic symbols
- **Native Compilation**: Compiles to native machine code via LLVM for high performance
- **User-Definable Patterns**: Extend the language with your own syntax through the pattern system
- **Multi-Paradigm**: Supports imperative, functional, and object-oriented programming styles

### Design Philosophy

3BX prioritizes readability and expressiveness. The pattern system allows developers to create domain-specific languages that read like natural English while compiling to efficient native code.

---

## Basic Syntax

### Comments

Comments start with `#` and continue to the end of the line:

```
# This is a comment
set x to 42  # This is an inline comment
```

### Variables

Variables are declared and assigned using the `set ... to ...` pattern:

```
set x to 42
set name to "Alice"
set pi to 3.14159
```

Variables are dynamically typed and do not require explicit type declarations.

### Data Types

#### Numbers

3BX supports both integers and floating-point numbers:

```
set count to 42          # Integer
set price to 19.99       # Floating-point
set negative to -10      # Negative numbers
```

#### Strings

Strings are enclosed in double quotes:

```
set greeting to "Hello, World!"
set message to "Line one"
```

### Arithmetic Operations

Basic arithmetic uses familiar operators:

| Operation      | Syntax          | Example           |
|---------------|-----------------|-------------------|
| Addition      | `left + right`  | `x + 5`           |
| Subtraction   | `left - right`  | `x - 3`           |
| Multiplication| `left * right`  | `x * 2`           |
| Division      | `left / right`  | `x / 4`           |

Example:

```
set a to 10
set b to 5
set sum to a + b           # 15
set difference to a - b    # 5
set product to a * b       # 50
set quotient to a / b      # 2
```

### Comparison Operators

Comparisons return boolean values:

| Comparison           | Syntax                              |
|---------------------|-------------------------------------|
| Equal               | `val1 is equal to val2`             |
| Not equal           | `val1 is not equal to val2`         |
| Less than           | `val1 is less than val2`            |
| Greater than        | `val1 is greater than val2`         |
| Less than or equal  | `val1 is less than or equal to val2`|
| Greater than or equal| `val1 is greater than or equal to val2`|

Example:

```
set x to 10
set y to 20

# These expressions return true/false
x is less than y           # true
x is equal to 10           # true
y is greater than x        # true
```

### Output

Print values to the console:

```
print "Hello, World!"
print x
write "Message" to the console
```

---

## Pattern System

The pattern system is the heart of 3BX's extensibility. Patterns allow you to define new syntax that integrates seamlessly with the language.

### Pattern Types

#### Effect Patterns

Effects perform actions without returning a value (side effects):

```
effect print value:
    when triggered:
        @intrinsic("print", value)

effect set var to val:
    when triggered:
        @intrinsic("store", var, val)
```

Usage:

```
print "Hello!"
set x to 42
```

#### Expression Patterns

Expressions return a value and include a `get:` block:

```
expression left + right:
    get:
        @intrinsic("add", left, right)

expression the sum of a and b:
    get:
        @intrinsic("add", a, b)
```

Usage:

```
set result to 10 + 5
set total to the sum of x and y
```

Expression patterns can also have a `set:` block for assignment:

```
expression memberName of object:
    get:
        @intrinsic("get_member", object, memberName)
    set:
        @intrinsic("set_member", object, memberName, val)
```

#### Section Patterns

Sections accept a code block as a parameter, enabling control structures:

```
section loop while {condition}:
    when triggered:
        @intrinsic("loop_while", condition, section)

section if {conditionvalue}:
    when triggered:
        @intrinsic("execute_if", conditionvalue, section)
```

Usage:

```
set x to 0
loop while {x < 10}:
    print x
    set x to x + 1

if {y is equal to 5}:
    print "y is five!"
```

#### Condition Patterns

Conditions are boolean expressions used in control flow:

```
condition if cond then:
    when triggered:
        @intrinsic("branch", cond)
```

### Pattern Syntax Details

#### Reserved Words

These words are treated as literals in pattern definitions:

```
set, to, if, then, else, while, loop, function, return,
is, the, a, an, and, or, not, pattern, syntax, when,
parsed, triggered, priority, import
```

When you define a pattern like `set var to val`, the words `set` and `to` are literals (they must appear exactly), while `var` and `val` are parameters.

#### Lazy Evaluation with Braces

Use `{param}` to capture an expression without immediately evaluating it:

```
section loop while {condition}:
    when triggered:
        @intrinsic("loop_while", condition, section)
```

The braces indicate that `condition` should be captured as a deferred expression, re-evaluated on each loop iteration.

#### Pattern Matching and Specificity

When multiple patterns could match, the most specific pattern wins. Specificity is determined by the number of literal words in the pattern.

```
# More specific (4 literals: "write", "to", "the", "console")
effect write msg to the console:
    when triggered:
        @intrinsic("print", msg)

# Less specific (2 literals: "print")
effect print value:
    when triggered:
        @intrinsic("print", value)
```

If two patterns have equal specificity and both match, the compiler emits an error.

### Pattern Blocks

#### `when triggered:` Block

Defines runtime behavior when the pattern is used:

```
effect greet name:
    when triggered:
        @intrinsic("print", name)
```

#### `when parsed:` Block

Defines compile-time behavior (for metaprogramming):

```
type:
    pattern:
        number
    when parsed:
        @intrinsic("primitive_type", "number")
```

#### `get:` and `set:` Blocks

Define how to read and write expression values:

```
expression x of vector:
    get:
        @intrinsic("get_member", vector, "x")
    set:
        @intrinsic("set_member", vector, "x", val)
```

---

## Intrinsics

Intrinsics are low-level operations that bridge patterns to LLVM code generation. They are the foundation on which all patterns are built.

### Variable Operations

| Intrinsic                    | Description                        |
|-----------------------------|-------------------------------------|
| `@intrinsic("store", var, val)` | Store a value in a variable      |
| `@intrinsic("load", var)`       | Load a value from a variable     |

### Arithmetic Operations

| Intrinsic                    | Description    |
|-----------------------------|----------------|
| `@intrinsic("add", a, b)`   | Addition       |
| `@intrinsic("sub", a, b)`   | Subtraction    |
| `@intrinsic("mul", a, b)`   | Multiplication |
| `@intrinsic("div", a, b)`   | Division       |

### Comparison Operations

| Intrinsic                    | Description              |
|-----------------------------|--------------------------|
| `@intrinsic("cmp_eq", a, b)`  | Equal to               |
| `@intrinsic("cmp_neq", a, b)` | Not equal to           |
| `@intrinsic("cmp_lt", a, b)`  | Less than              |
| `@intrinsic("cmp_gt", a, b)`  | Greater than           |
| `@intrinsic("cmp_lte", a, b)` | Less than or equal     |
| `@intrinsic("cmp_gte", a, b)` | Greater than or equal  |

### Control Flow Operations

| Intrinsic                                  | Description                              |
|-------------------------------------------|------------------------------------------|
| `@intrinsic("execute", block)`            | Execute a code block                     |
| `@intrinsic("evaluate", expr)`            | Evaluate a lazy expression               |
| `@intrinsic("execute_if", condition, block)` | Conditionally execute a block         |
| `@intrinsic("loop_while", condition, block)` | Loop while condition is true          |
| `@intrinsic("branch", cond)`              | Branch based on condition                |
| `@intrinsic("return", val)`               | Return a value from a function           |

### I/O Operations

| Intrinsic                      | Description                       |
|-------------------------------|-----------------------------------|
| `@intrinsic("print", value)`  | Print a value to the console     |
| `@intrinsic("print_labeled", label, value)` | Print with a label     |

### Foreign Function Interface (FFI)

| Intrinsic                                        | Description                    |
|-------------------------------------------------|--------------------------------|
| `@intrinsic("call", "LIBRARY", "function", args...)` | Call an external function |

Example:

```
# Call a C library function
@intrinsic("call", "glfw3", "glfwInit")

# Call with arguments
@intrinsic("call", "GL", "glClearColor", 0.0, 0.0, 0.0, 1.0)
```

### Type Operations

| Intrinsic                              | Description                    |
|---------------------------------------|--------------------------------|
| `@intrinsic("primitive_type", name)`  | Define a primitive type        |
| `@intrinsic("type_check", value, type)` | Check if value is of type    |
| `@intrinsic("get_type", value)`       | Get the type of a value        |
| `@intrinsic("cast", value, type)`     | Cast value to a type           |
| `@intrinsic("optional_type", type)`   | Create an optional type        |
| `@intrinsic("list_type", type)`       | Create a list type             |
| `@intrinsic("type_alias", name, type)` | Create a type alias           |

### Object Operations

| Intrinsic                                      | Description                    |
|-----------------------------------------------|--------------------------------|
| `@intrinsic("create_instance", className)`    | Create a class instance        |
| `@intrinsic("get_member", object, memberName)` | Get a member value            |
| `@intrinsic("set_member", object, member, val)` | Set a member value           |
| `@intrinsic("call_method", object, methodName)` | Call an object method        |

### Event Operations

| Intrinsic                                        | Description                    |
|-------------------------------------------------|--------------------------------|
| `@intrinsic("register_handler", event, handler)` | Register an event handler    |
| `@intrinsic("fire_event", eventName)`            | Fire an event                |
| `@intrinsic("fire_event_with_data", event, data)` | Fire an event with data     |
| `@intrinsic("cancel_current_event")`             | Cancel the current event     |
| `@intrinsic("get_event_data")`                   | Get current event data       |

---

## Standard Library

### prelude.3bx - Core Language Patterns

The prelude provides essential patterns that form the foundation of 3BX:

```
# Variables
effect set var to val:
    when triggered:
        @intrinsic("store", var, val)

# Output
effect write msg to the console:
    when triggered:
        @intrinsic("print", msg)

# Arithmetic
expression left + right:
    get:
        @intrinsic("add", left, right)

expression left - right:
    get:
        @intrinsic("sub", left, right)

expression left * right:
    get:
        @intrinsic("mul", left, right)

expression left / right:
    get:
        @intrinsic("div", left, right)

# Comparisons
expression val1 is equal to val2:
    get:
        @intrinsic("cmp_eq", val1, val2)

expression val1 is less than val2:
    get:
        @intrinsic("cmp_lt", val1, val2)

expression val1 is greater than val2:
    get:
        @intrinsic("cmp_gt", val1, val2)

# Return
effect return val:
    when triggered:
        @intrinsic("return", val)
```

### loops.3bx - Loop Constructs

The loops library provides various iteration patterns:

#### While Loop

```
section loop while {condition}:
    when triggered:
        @intrinsic("loop_while", condition, section)
```

Usage:

```
set x to 0
loop while {x < 10}:
    print x
    set x to x + 1
```

#### Repeat N Times

```
section loop count times:
    when triggered:
        set loopindex to 0
        loop while {loopindex < count}:
            @intrinsic("execute", section)
            set loopindex to loopindex + 1
```

Usage:

```
loop 5 times:
    print "Hello!"
```

The variable `loopindex` (0-indexed) is available inside the loop.

#### Do-While Loop

```
section do then loop while {condition}:
    when triggered:
        @intrinsic("execute", section)
        @intrinsic("loop_while", condition, section)
```

Usage:

```
set x to 0
do then loop while {x < 5}:
    print x
    set x to x + 1
```

#### Range Loops

Exclusive range (does not include end value):

```
section loop from startval to endval:
    when triggered:
        set loopindex to startval
        loop while {loopindex < endval}:
            @intrinsic("execute", section)
            set loopindex to loopindex + 1
```

Usage:

```
loop from 1 to 5:
    print loopindex    # Prints 1, 2, 3, 4
```

Inclusive range (includes end value):

```
section loop from startval through endval:
    when triggered:
        set loopindex to startval
        set limit to endval + 1
        loop while {loopindex < limit}:
            @intrinsic("execute", section)
            set loopindex to loopindex + 1
```

Usage:

```
loop from 1 through 5:
    print loopindex    # Prints 1, 2, 3, 4, 5
```

#### Step Loop

```
section loop from startval to endval by stepval:
    when triggered:
        set loopindex to startval
        loop while {loopindex < endval}:
            @intrinsic("execute", section)
            set loopindex to loopindex + stepval
```

Usage:

```
loop from 0 to 10 by 2:
    print loopindex    # Prints 0, 2, 4, 6, 8
```

#### Countdown Loop

```
section countdown from startval:
    when triggered:
        set loopindex to startval
        loop while {loopindex > 0}:
            @intrinsic("execute", section)
            set loopindex to loopindex - 1
```

Usage:

```
countdown from 5:
    print loopindex    # Prints 5, 4, 3, 2, 1
```

#### Loop Until

```
section loop until {condition}:
    when triggered:
        set shouldcontinue to 1
        @intrinsic("execute_if", condition, {set shouldcontinue to 0})
        loop while {shouldcontinue == 1}:
            @intrinsic("execute", section)
            set shouldcontinue to 1
            @intrinsic("execute_if", condition, {set shouldcontinue to 0})
```

Usage:

```
set x to 0
loop until {x == 5}:
    print x
    set x to x + 1
```

#### Forever Loop

```
section loop forever:
    when triggered:
        @intrinsic("loop_while", {1}, section)
```

Usage (use with caution - must have a break mechanism):

```
loop forever:
    # Infinite loop
    poll events
```

### print.3bx - Output Operations

```
effect print value:
    when triggered:
        @intrinsic("print", value)

effect print msg to the console:
    when triggered:
        @intrinsic("print", msg)

effect print label value:
    when triggered:
        @intrinsic("print_labeled", label, value)
```

Usage:

```
print "Hello, World!"
print x
print "Result:" result
```

### opengl.3bx - Graphics Library

The OpenGL library provides patterns for window management and rendering.

#### Window Management

```
# Initialize the graphics system
initialize graphics

# Create a window
set mainWindow to @intrinsic("call", "glfw3", "glfwCreateWindow", 800, 600, "My App", 0, 0)

# Make window the current context
make window mainWindow the current context

# Enable/disable vsync
enable vsync
disable vsync

# Event handling
poll events
wait for events

# Swap buffers (double buffering)
swap buffers of window mainWindow

# Clean up
terminate graphics
```

#### Drawing Operations

```
# Clear the screen
clear screen with color 0.1 0.2 0.3          # RGB
clear screen with color 0.1 0.2 0.3 1.0      # RGBA

# Set drawing color
set draw color to 1.0 0.0 0.0                # Red
set draw color to 1.0 1.0 1.0 0.5            # White, 50% transparent

# Draw shapes
draw rectangle at 100 100 with width 50 and height 30
draw triangle at 0 0 and 100 0 and 50 100
draw line from 0 0 to 100 100
```

#### Matrix Operations

```
# Set matrix mode
set matrix mode to projection
set matrix mode to modelview

# Reset matrix
reset matrix

# Transformations
translate by 10 20 0
rotate by 45 around 0 0 1
scale by 2 2 1

# Matrix stack
push matrix
pop matrix

# Orthographic projection
set orthographic projection 0 800 600 0 -1 1
```

#### Primitive Drawing

```
# Low-level drawing
begin drawing triangles
add vertex at 0 0
add vertex at 100 0
add vertex at 50 100
end drawing

begin drawing quads
add vertex at 0 0
add vertex at 100 0
add vertex at 100 100
add vertex at 0 100
end drawing
```

#### Input Handling

```
# Keyboard
expression key k is pressed in window w:
    get:
        @intrinsic("call", "glfw3", "glfwGetKey", w, k)

# Key constants
KEY_ESCAPE    # 256
KEY_SPACE     # 32
KEY_ENTER     # 257
KEY_UP        # 265
KEY_DOWN      # 264
KEY_LEFT      # 263
KEY_RIGHT     # 262

# Mouse
expression mouse button b is pressed in window w:
    get:
        @intrinsic("call", "glfw3", "glfwGetMouseButton", w, b)

MOUSE_LEFT    # 0
MOUSE_RIGHT   # 1
MOUSE_MIDDLE  # 2
```

### class.3bx - Object-Oriented Programming

```
# Define a class
class:
    pattern:
        vector
    members:
        x, y, z
    when created:
        set each member to 0

# Create instances
expression a new className:
    get:
        @intrinsic("create_instance", className)

# Access members
expression memberName of object:
    get:
        @intrinsic("get_member", object, memberName)

effect set memberName of object to value:
    when triggered:
        @intrinsic("set_member", object, memberName, value)
```

Usage:

```
set v to a new vector
set x of v to 10
set y of v to 20
print x of v    # 10
```

### vector.3bx - 3D Vector Mathematics

```
# Create vectors
set v to vector 1 2 3
set origin to zero vector
set right to unit x vector

# Vector operations
set sum to add vecA to vecB
set diff to subtract vecB from vecA
set scaled to multiply vec by 2
set normalized to normalized vec

# Dot and cross products
set d to vecA dot vecB
set c to vecA cross vecB
set d to dot product of vecA and vecB
set c to cross product of vecA and vecB

# Magnitude and distance
set len to magnitude of vec
set dist to distance between vecA and vecB
```

### type.3bx - Type System

Primitive types:

```
type:
    pattern: number
type:
    pattern: integer
type:
    pattern: decimal
type:
    pattern: text
type:
    pattern: boolean
type:
    pattern: nothing
```

Type operations:

```
# Type checking
value is a number
value is an integer

# Type casting
value as number
convert value to text

# Optional types
optional number
number or nothing

# List types
list of numbers
```

### event.3bx - Event System

```
# Register handlers
on eventName:
    # handler code

when eventName:
    # handler code

# Fire events
fire eventName
trigger eventName
emit eventName

# Fire with data
fire eventName with eventData

# One-time handlers
once eventName:
    # runs only once

# Cancel events
cancel the event
event is cancelled

# Access event data
the event
event data
fieldName of the event
```

Built-in events:

- `tick` - Called every frame (provides `delta time`)
- `startup` - Called when program starts
- `shutdown` - Called when program ends
- `error occurred` - Called on errors (provides `message`, `source`)

---

## Examples

### Hello World

```
# A simple 3BX example
set x to 42
set y to 10
set result to x + y

write result to the console
```

### Calculator

This example demonstrates defining arithmetic patterns:

```
# calculator.3bx - A simple calculator demonstrating 3BX patterns

# === Arithmetic Patterns ===

pattern:
    syntax: add a and b
    when triggered:
        @intrinsic("add", a, b)

pattern:
    syntax: subtract b from a
    when triggered:
        @intrinsic("sub", a, b)

pattern:
    syntax: multiply a by b
    when triggered:
        @intrinsic("mul", a, b)

pattern:
    syntax: divide a by b
    when triggered:
        @intrinsic("div", a, b)

# === Alternative Natural Language Patterns ===

pattern:
    syntax: the sum of a and b
    when triggered:
        @intrinsic("add", a, b)

pattern:
    syntax: the product of a and b
    when triggered:
        @intrinsic("mul", a, b)

# === Example Usage ===

set x to 10
set y to 5

set sum to add x and y
print sum                          # 15

set difference to subtract y from x
print difference                   # 5

set product to multiply x by y
print product                      # 50

set quotient to divide x by y
print quotient                     # 2

# Using natural language patterns
set result1 to the sum of 20 and 30
print result1                      # 50

set result2 to the product of 7 and 8
print result2                      # 56
```

### Custom Loops

This example shows how to define control flow patterns:

```
# Custom Loops Example

# Section pattern for if statements
section if {conditionvalue}:
    when triggered:
        @intrinsic("execute_if", conditionvalue, section)

# While loop using the loop_while intrinsic
section loop while {condition}:
    when triggered:
        @intrinsic("loop_while", condition, section)

# Loop N times
section loop loopcount times:
    when triggered:
        set loopindex to 0
        loop while {loopindex < loopcount}:
            @intrinsic("execute", section)
            set loopindex to loopindex + 1

# Range loop
section loop from startval to endval:
    when triggered:
        set loopindex to startval
        loop while {loopindex < endval}:
            @intrinsic("execute", section)
            set loopindex to loopindex + 1

# Example usage:

print "Looping 5 times:"
loop 5 times:
    print "  Hello from loop!"

print "While loop from 0 to 4:"
set x to 0
loop while {x < 5}:
    print x
    set x to x + 1

print "Testing if statement:"
set y to 10
if {y == 10}:
    print "  y is 10!"

print "Range loop from 1 to 5:"
loop from 1 to 5:
    print loopindex
```

### FFI (Foreign Function Interface)

This example shows how to call external C functions:

```
# test_ffi.3bx - Test FFI functionality

# Import external C functions
import function puts(str) from "stdio.h"
import function sqrt(x) from "math.h"

# Create a pattern that wraps the external function call
effect say message:
    when triggered:
        @intrinsic("call", "puts", message)

# Test: call the pattern
say "Hello from FFI!"

print "FFI test complete"
```

### GUI Calculator

This example demonstrates OpenGL graphics programming:

```
# calculator_gui.3bx - GUI Calculator using OpenGL

import lib/prelude.3bx
import lib/print.3bx
import lib/opengl.3bx

# Window settings
set windowWidth to 320
set windowHeight to 480
set buttonSize to 70

# Initialize graphics
initialize graphics

# Create window
set mainWindow to @intrinsic("call", "glfw3", "glfwCreateWindow",
                              windowWidth, windowHeight, "3BX Calculator", 0, 0)

make window mainWindow the current context
enable vsync

# Set up 2D projection
set matrix mode to projection
reset matrix
set orthographic projection 0 windowWidth windowHeight 0 -1 1
set matrix mode to modelview
reset matrix

enable blending

# Draw calculator interface
poll events
clear screen with color 0.1 0.1 0.15

# Draw display background
set draw color to 0.2 0.2 0.25
draw rectangle at 10 10 with width 300 and height 70

# Draw buttons
set draw color to 0.3 0.3 0.35
draw rectangle at 15 100 with width buttonSize and height buttonSize

swap buffers of window mainWindow
terminate graphics
```

---

## Implementation Status

### Working Features

- Basic statements: `set x to 42`, arithmetic expressions
- Pattern definitions parsed and registered
- `@intrinsic` calls parsed and executed
- LLVM IR generation
- Main function wrapper for top-level code

### In Progress

- Pattern-based statement matching
- Prelude loading
- Import system

### Not Yet Implemented

- Proper indentation-based blocks
- Control flow (if/else, while) - available via patterns
- User-defined functions
- Type inference for patterns
- Exhaustive pattern matching

---

## Appendix: Reserved Words

The following words are reserved and treated as literals in pattern definitions:

```
set, to, if, then, else, while, loop, function, return,
is, the, a, an, and, or, not, pattern, syntax, when,
parsed, triggered, priority, import
```

---

## Appendix: File Extensions

| Extension | Description           |
|-----------|----------------------|
| `.3bx`    | 3BX source files     |
| `.cpp`    | C++ implementation   |
| `.hpp`    | C++ headers          |
