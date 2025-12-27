# Welcome to 3BX!

**3BX** is a programming language that reads like English. Instead of cryptic symbols and confusing keywords, you write code that actually makes sense!

## What Makes 3BX Different?

Most programming languages look like this:
```javascript
let x = 5;
console.log(x + 3);
```

3BX looks like this:
```
set x to 5
print x + 3
```

See the difference? 3BX uses natural language so you can focus on **what** you want to do, not **how** to write it.

## The Magic of Patterns

The coolest thing about 3BX is that YOU can teach it new commands! These are called "patterns." Want to add a command like `say hello to Alex`? You can create that yourself!

```
pattern:
    syntax: say hello to name
    when triggered:
        print "Hello,"
        print name
```

Now `say hello to Alex` is a valid command in your program!

## Quick Links

| Guide | What You'll Learn |
|-------|-------------------|
| [Getting Started](Getting-Started.md) | Install 3BX and run your first program |
| [Writing Your First Program](Writing-Your-First-Program.md) | Learn the basics step by step |
| [Patterns](Patterns.md) | Create your own custom commands |
| [Examples](Examples.md) | See what you can build |

## Why Learn 3BX?

1. **Easy to Read** - Your code looks like normal sentences
2. **Customizable** - Create your own commands with patterns
3. **Powerful** - Compiles to super-fast native code using LLVM
4. **Fun** - Start building cool stuff right away!

## Quick Example

Here's a tiny program that calculates and prints a result:

```
# My first 3BX program!
set apples to 5
set oranges to 3
set total_fruit to apples + oranges

print "I have this many fruits:"
print total_fruit
```

This will output:
```
I have this many fruits:
8
```

Ready to start? Head over to [Getting Started](Getting-Started.md)!
