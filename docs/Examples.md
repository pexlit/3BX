# Examples

Here are some examples to show what 3BX can do. Try them out, modify them, and make them your own.

## 1. Hello World

```
# hello.3bx
print "Hello, World!"
print "Welcome to 3BX!"
```

## 2. Simple Calculator

```
# calculator.3bx

set a to 20
set b to 5

print "Let's do some math!"
print "==================="

print "a + b ="
print a + b

print "a - b ="
print a - b

print "a * b ="
print a * b

print "a / b ="
print a / b
```

## 3. Temperature Converter

```
# temperature.3bx

# Define a conversion pattern
effect fahrenheit from celsius c:
    execute:
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

## 4. Custom Greeting System

```
# greetings.3bx

# Define different greeting styles
effect greet name formally:
    execute:
        print "Good day,"
        print name
        print "It is a pleasure to meet you."

effect greet name casually:
    execute:
        print "Hey"
        print name
        print "What's up?"

effect greet name excitedly:
    execute:
        print "WOW!"
        print name
        print "SO HAPPY TO SEE YOU!"

# Try them out
greet "Dr. Smith" formally
greet "Alex" casually
greet "Best Friend" excitedly
```

## 5. Area Calculator

```
# shapes.3bx

# Rectangle pattern
effect area of rectangle width w height h:
    execute:
        set result to w * h
        print "Rectangle area:"
        print result

# Square pattern
effect area of square side s:
    execute:
        set result to s * s
        print "Square area:"
        print result

# Calculate some areas
area of rectangle width 10 height 5
area of square side 7

# Expression version that returns a value
expression the area of box w by h:
    get:
        return @intrinsic("mul", w, h)

set room_size to the area of box 12 by 15
print "My room is this many square feet:"
print room_size
```

## 6. Math Helper with Natural Language

```
# math_helper.3bx

# Natural language expressions
expression add a and b:
    get:
        return @intrinsic("add", a, b)

expression subtract b from a:
    get:
        return @intrinsic("sub", a, b)

expression multiply a by b:
    get:
        return @intrinsic("mul", a, b)

expression divide a by b:
    get:
        return @intrinsic("div", a, b)

expression the square of x:
    get:
        return @intrinsic("mul", x, x)

# Try them out
print "Add 15 and 25:"
set sum to add 15 and 25
print sum

print "The square of 8:"
set sq to the square of 8
print sq
```

## 7. Using Optional Elements

```
# optional.3bx

# Optional "the" using empty alternative
effect show [|the] item:
    execute:
        print item

# Both work:
show "apple"
show the "banana"

# Required choice between articles
effect display [a|an|the] thing:
    execute:
        print thing

# Must use one of the articles:
display a "car"
display an "elephant"
display the "house"
```

## Running These Examples

1. Copy any example into a file with a `.3bx` extension
2. Make sure you're in the 3BX `build` folder
3. Run: `./3bx example.3bx`

## Create Your Own

The best way to learn is to experiment. Try:

- Make a calculator with natural language patterns
- Create custom commands for your specific domain
- Build unit converters (miles to kilometers, etc.)

## More Resources

- [Writing Your First Program](Writing-Your-First-Program.md) - Back to basics
- [Patterns](Patterns.md) - Learn more about creating patterns
- [Home](Home.md) - Back to the main page
