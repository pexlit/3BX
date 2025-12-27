# Patterns - Create Your Own Commands

Patterns are the superpower of 3BX! They let you teach the language new commands that look like plain English. This is what makes 3BX unique.

## What's a Pattern?

Think of a pattern as teaching 3BX a new trick. You define what the command looks like and what it should do.

For example, you could teach 3BX what "say hello" means:

```
pattern:
    syntax: say hello
    when triggered:
        print "Hello there!"
```

Now you can use it in your program:
```
say hello
```

And it prints: `Hello there!`

## The Structure of a Pattern

Every pattern has three parts:

```
pattern:
    syntax: your command here
    when triggered:
        what happens when the command runs
```

1. **`pattern:`** - Tells 3BX "I'm defining a new command"
2. **`syntax:`** - What the command looks like when you type it
3. **`when triggered:`** - What the command actually does

## Patterns with Parameters

The real power comes when you add **parameters** - values that the user fills in. Any word in your syntax that isn't a "reserved word" becomes a parameter:

```
pattern:
    syntax: greet name
    when triggered:
        print "Hello,"
        print name
```

Now you can use it like this:
```
greet "Alex"
greet "Sam"
greet "Taylor"
```

Each time, `name` gets replaced with whatever you put there!

## Reserved Words

Some words have special meaning in 3BX and stay fixed in your pattern:

`set`, `to`, `if`, `then`, `else`, `while`, `loop`, `is`, `the`, `a`, `an`, `and`, `or`, `not`, `return`, `import`

These words become part of your pattern's structure:

```
pattern:
    syntax: add x to y
    when triggered:
        # "to" is reserved, so only x and y are parameters
        print x + y
```

Usage:
```
add 5 to 10    # Prints: 15
```

## Types of Patterns

### Effect Patterns (Do Something)

These patterns DO something, like print or save:

```
effect greet name nicely:
    when triggered:
        print "Hello there,"
        print name
        print "Nice to see you!"
```

### Expression Patterns (Calculate Something)

These patterns CALCULATE a value that you can use:

```
expression double of x:
    get:
        x * 2
```

Usage:
```
set result to double of 5
print result    # Prints: 10
```

### Section Patterns (Contain Code Blocks)

These patterns contain other code inside them:

```
section repeat count times:
    when triggered:
        set i to 0
        loop while {i < count}:
            @intrinsic("execute", section)
            set i to i + 1
```

Usage:
```
repeat 3 times:
    print "Hip hip hooray!"
```

## Intrinsics - The Building Blocks

Intrinsics are the basic operations that patterns are built from. Think of them as the "atoms" of 3BX:

| Intrinsic | What It Does |
|-----------|--------------|
| `@intrinsic("print", val)` | Print a value |
| `@intrinsic("store", var, val)` | Store a value in a variable |
| `@intrinsic("load", var)` | Get a value from a variable |
| `@intrinsic("add", a, b)` | Add two numbers |
| `@intrinsic("sub", a, b)` | Subtract b from a |
| `@intrinsic("mul", a, b)` | Multiply two numbers |
| `@intrinsic("div", a, b)` | Divide a by b |
| `@intrinsic("execute", section)` | Run a section of code |

Here's how `print` is actually defined:

```
effect print value:
    when triggered:
        @intrinsic("print", value)
```

## A Complete Example: Custom Math

Let's create some natural language math commands:

```
# Define our patterns
pattern:
    syntax: add a and b
    when triggered:
        @intrinsic("add", a, b)

pattern:
    syntax: the sum of a and b
    when triggered:
        @intrinsic("add", a, b)

pattern:
    syntax: multiply a by b
    when triggered:
        @intrinsic("mul", a, b)

pattern:
    syntax: the product of a and b
    when triggered:
        @intrinsic("mul", a, b)

# Now use them!
set result1 to add 10 and 5
print result1    # 15

set result2 to the sum of 20 and 30
print result2    # 50

set result3 to multiply 7 by 8
print result3    # 56

set result4 to the product of 3 and 4
print result4    # 12
```

See how natural that reads? You're writing math in English!

## Another Example: Area Calculations

```
pattern:
    syntax: the area of rectangle with width w and height h
    when triggered:
        @intrinsic("mul", w, h)

pattern:
    syntax: the area of square with side s
    when triggered:
        @intrinsic("mul", s, s)

# Calculate some areas
set room_area to the area of rectangle with width 10 and height 15
print "Room area:"
print room_area    # 150

set tile_area to the area of square with side 5
print "Tile area:"
print tile_area    # 25
```

## Tips for Writing Great Patterns

1. **Make them readable** - Write patterns that sound like normal sentences
   - Good: `the area of rectangle with width x and height y`
   - Not as good: `rect area x y`

2. **Use descriptive parameter names**
   - Good: `greet person_name with message`
   - Not as good: `greet n with m`

3. **Keep them focused** - Each pattern should do one thing well

4. **Use reserved words** - Words like `the`, `and`, `with`, `to` make patterns more readable

5. **Test as you go** - Try your pattern right after writing it to make sure it works

## How 3BX Chooses Between Patterns

If two patterns could match the same input, 3BX picks the more specific one (the one with more reserved/literal words). If they're equally specific, you'll get an error.

## What's Next?

- [Examples](Examples.md) - See patterns used in real programs
- [Writing Your First Program](Writing-Your-First-Program.md) - Back to basics
