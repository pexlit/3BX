# Writing Your First Program

Let's learn 3BX step by step! By the end of this tutorial, you'll know how to store values, do math, print output, and even create loops.

## Storing Values with Variables

A **variable** is like a labeled box where you can store information. In 3BX, you create variables using `set`:

```
set my_number to 42
```

This creates a box labeled `my_number` and puts the value `42` inside it.

You can store different types of things:

```
# Whole numbers
set age to 15

# Decimal numbers
set price to 9.99

# Text (called "strings")
set name to "Alex"
```

**Tip:** Variable names should describe what they store. `player_score` is better than `x`!

## Printing Things to the Screen

To show something on the screen, use `print`:

```
print "Hello there!"
print 42
print my_number
```

Each `print` goes on a new line. Try this program:

```
set greeting to "Welcome to 3BX!"
print greeting
print "Let's learn together."
```

## Comments - Notes for Yourself

Lines starting with `#` are **comments**. The computer ignores them - they're just notes for humans:

```
# This is a comment - the computer skips it
print "This gets printed"

# Comments help you remember what your code does
set player_health to 100  # Player starts with full health
```

Use comments to explain tricky parts of your code!

## Doing Math

3BX can do all kinds of math:

```
set a to 10
set b to 5

# Addition
print a + b    # Shows: 15

# Subtraction
print a - b    # Shows: 5

# Multiplication
print a * b    # Shows: 50

# Division
print a / b    # Shows: 2
```

You can also save the result of math in a new variable:

```
set width to 10
set height to 5
set area to width * height

print "The area is:"
print area
```

## Your First Real Program: A Calculator

Let's put it all together! Here's a program that adds two numbers:

```
# ================================
# My First Calculator
# This program adds two numbers together
# ================================

# Set up our numbers
set first_number to 10
set second_number to 25

# Do the math
set result to first_number + second_number

# Show the results
print "The first number is:"
print first_number

print "The second number is:"
print second_number

print "When you add them together, you get:"
print result
```

Save this as `calculator.3bx` and run it:
```bash
./3bx calculator.3bx
```

## Loops - Doing Things Multiple Times

What if you want to do something over and over? That's where **loops** come in!

### Counting with a Loop

```
# Count from 0 to 4
set counter to 0

loop while {counter < 5}:
    print counter
    set counter to counter + 1

print "Done counting!"
```

This will print:
```
0
1
2
3
4
Done counting!
```

**How it works:**
1. Start with `counter` at 0
2. Check if `counter < 5` - if yes, run the code inside
3. Print the counter and add 1 to it
4. Go back to step 2
5. When `counter` reaches 5, the loop stops

### Looping a Set Number of Times

Want to do something exactly 5 times? There's an easier way:

```
loop 5 times:
    print "Hello!"
```

This prints "Hello!" five times. Simple!

### Counting Through a Range

To count from one number to another:

```
loop from 1 to 5:
    print loopindex
```

This prints: 1, 2, 3, 4 (stops before 5)

**Note:** The special variable `loopindex` tells you which iteration you're on.

## Conditionals - Making Decisions

Sometimes you want your program to make decisions. Use the `if` statement:

```
set score to 85

if {score >= 90}:
    print "You got an A!"

if {score >= 80}:
    print "You got a B!"

if {score < 80}:
    print "Keep practicing!"
```

The code inside the `if` only runs when the condition in `{braces}` is true.

## Putting It All Together

Here's a bigger program that uses everything we learned:

```
# ================================
# Multiplication Table Generator
# ================================

print "Multiplication table for 5:"
print "=========================="

set multiplier to 5

loop from 1 to 11:
    set result to multiplier * loopindex
    print loopindex
    print "times 5 equals"
    print result
    print "---"
```

## Quick Reference

| What You Want to Do | How to Write It |
|---------------------|-----------------|
| Store a value | `set name to value` |
| Print something | `print value` |
| Add numbers | `a + b` |
| Subtract numbers | `a - b` |
| Multiply numbers | `a * b` |
| Divide numbers | `a / b` |
| Loop while true | `loop while {condition}:` |
| Loop N times | `loop N times:` |
| Loop through range | `loop from X to Y:` |
| Check a condition | `if {condition}:` |
| Write a comment | `# your comment` |

## Try It Yourself!

Here are some challenges to practice:

1. **Modify the calculator** to do subtraction instead of addition
2. **Create a countdown** from 10 to 1 (hint: use `loop while` or check out `countdown from`)
3. **Make a table** showing squares of numbers from 1 to 10

## What's Next?

Ready to create your own commands? Check out:
- [Patterns](Patterns.md) - Create your own custom commands
- [Examples](Examples.md) - See bigger programs
