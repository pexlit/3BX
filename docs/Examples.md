# Examples

Here are some fun examples to show you what 3BX can do! Try them out, modify them, and make them your own.

## 1. Hello World - The Classic

Every programmer starts here:

```
# hello.3bx - The simplest 3BX program

print "Hello, World!"
print "Welcome to 3BX!"
```

**What it does:** Prints two lines of text to the screen.

## 2. Simple Calculator

A program that does basic math:

```
# calculator.3bx - Basic math operations

set a to 20
set b to 5

print "Let's do some math!"
print "==================="

print "a ="
print a

print "b ="
print b

print "a + b ="
print a + b

print "a - b ="
print a - b

print "a * b ="
print a * b

print "a / b ="
print a / b
```

**Output:**
```
Let's do some math!
===================
a =
20
b =
5
a + b =
25
a - b =
15
a * b =
100
a / b =
4
```

## 3. Temperature Converter

Convert temperatures from Celsius to Fahrenheit:

```
# temperature.3bx - Temperature conversion

# Define a conversion pattern
pattern:
    syntax: fahrenheit from celsius c
    when triggered:
        # Formula: F = C * 9/5 + 32
        set temp to c * 9
        set temp to temp / 5
        set temp to temp + 32
        print temp

# Convert some temperatures
print "0 Celsius in Fahrenheit:"
fahrenheit from celsius 0

print "25 Celsius in Fahrenheit:"
fahrenheit from celsius 25

print "100 Celsius in Fahrenheit:"
fahrenheit from celsius 100
```

## 4. Counting Loop

Count from 1 to 10:

```
# counting.3bx - Learn to count!

print "Let's count to 10!"
print "=================="

set counter to 1

loop while {counter <= 10}:
    print counter
    set counter to counter + 1

print "=================="
print "Done!"
```

## 5. Multiplication Table

Generate a times table:

```
# times_table.3bx - Generate a multiplication table

set number to 7

print "Times table for:"
print number
print "=================="

loop from 1 to 11:
    set result to number * loopindex
    print loopindex
    print "x"
    print number
    print "="
    print result
    print "---"
```

## 6. Custom Greeting System

Create personalized greetings with patterns:

```
# greetings.3bx - Custom greeting patterns

# Define different greeting styles
pattern:
    syntax: greet name formally
    when triggered:
        print "Good day,"
        print name
        print "It is a pleasure to meet you."

pattern:
    syntax: greet name casually
    when triggered:
        print "Hey"
        print name
        print "What's up?"

pattern:
    syntax: greet name excitedly
    when triggered:
        print "WOW!"
        print name
        print "SO HAPPY TO SEE YOU!"

# Try them out
greet "Dr. Smith" formally
print ""
greet "Alex" casually
print ""
greet "Best Friend" excitedly
```

## 7. Area Calculator

Calculate areas of different shapes:

```
# shapes.3bx - Calculate areas of shapes

# Rectangle pattern
pattern:
    syntax: area of rectangle width w height h
    when triggered:
        set result to w * h
        print "Rectangle area:"
        print result

# Square pattern (a square is a special rectangle!)
pattern:
    syntax: area of square side s
    when triggered:
        set result to s * s
        print "Square area:"
        print result

# Calculate some areas
area of rectangle width 10 height 5
area of square side 7

# Store in a variable for later use
pattern:
    syntax: the area of box w by h
    when triggered:
        @intrinsic("mul", w, h)

set room_size to the area of box 12 by 15
print "My room is this many square feet:"
print room_size
```

## 8. Countdown Timer

Count down from 10 to liftoff:

```
# countdown.3bx - Rocket launch countdown

print "Initiating launch sequence..."
print ""

set count to 10

loop while {count >= 1}:
    print count
    set count to count - 1

print ""
print "LIFTOFF!"
print "We have liftoff!"
```

## 9. Math Helper with Natural Language

Create intuitive math commands:

```
# math_helper.3bx - Natural language math

# Addition
pattern:
    syntax: add a and b together
    when triggered:
        @intrinsic("add", a, b)

# Subtraction
pattern:
    syntax: subtract b from a
    when triggered:
        @intrinsic("sub", a, b)

# Multiplication
pattern:
    syntax: multiply a by b
    when triggered:
        @intrinsic("mul", a, b)

# Division
pattern:
    syntax: divide a by b
    when triggered:
        @intrinsic("div", a, b)

# Square
pattern:
    syntax: the square of x
    when triggered:
        @intrinsic("mul", x, x)

# Cube
pattern:
    syntax: the cube of x
    when triggered:
        set squared to x * x
        @intrinsic("mul", squared, x)

# Try them out!
print "Add 15 and 25 together:"
set sum to add 15 and 25 together
print sum

print "The square of 8:"
set sq to the square of 8
print sq

print "The cube of 3:"
set cb to the cube of 3
print cb
```

## 10. FizzBuzz Challenge

The classic programming challenge - print numbers 1-20, but:
- For multiples of 3, print "Fizz"
- For multiples of 5, print "Buzz"
- For multiples of both, print "FizzBuzz"

```
# fizzbuzz.3bx - The classic challenge

print "FizzBuzz from 1 to 20"
print "====================="

set n to 1

loop while {n <= 20}:
    set div3 to n / 3
    set mult3 to div3 * 3

    set div5 to n / 5
    set mult5 to div5 * 5

    set div15 to n / 15
    set mult15 to div15 * 15

    # Check divisibility (if n equals the multiple, it's divisible)
    if {mult15 == n}:
        print "FizzBuzz"

    if {mult3 == n}:
        if {mult15 != n}:
            print "Fizz"

    if {mult5 == n}:
        if {mult15 != n}:
            print "Buzz"

    if {mult3 != n}:
        if {mult5 != n}:
            print n

    set n to n + 1
```

## Running These Examples

1. Copy any example into a file with a `.3bx` extension (like `example.3bx`)
2. Make sure you're in the 3BX `build` folder
3. Run: `./3bx example.3bx`

## Challenge Ideas

Try modifying these examples:

1. **Calculator:** Add support for more operations (like power or modulo)
2. **Temperature:** Convert Fahrenheit to Celsius instead
3. **Countdown:** Make it count by 2s instead of 1s
4. **Times Table:** Let the user specify which number to make a table for
5. **Shapes:** Add a triangle area calculator (base * height / 2)

## Create Your Own!

The best way to learn is to experiment. Try combining ideas from different examples:

- Make a calculator that uses natural language patterns
- Create a game that uses loops and conditionals
- Build a unit converter (miles to kilometers, pounds to kilograms)

## More Resources

- [Writing Your First Program](Writing-Your-First-Program.md) - Back to basics
- [Patterns](Patterns.md) - Learn more about creating patterns
- [Home](Home.md) - Back to the main page
