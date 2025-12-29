# Getting Started

This guide will help you install 3BX and run your first program. Don't worry - it's easier than you think!

## What You'll Need

Before we start, make sure you have these installed on your computer:
- **Git** - to download the code
- **CMake** - to set up the build
- **A C++ compiler** - like g++ or clang
- **LLVM** - the engine that makes your code run fast

### Installing on Ubuntu/Debian

Open a terminal and paste this command:
```bash
sudo apt update && sudo apt install -y git llvm llvm-dev clang cmake build-essential libz-dev libzstd-dev
```

### Installing on Fedora

```bash
sudo dnf install git llvm llvm-devel clang cmake gcc-c++ zlib-devel libzstd-devel
```

### Installing on Arch Linux

```bash
sudo pacman -S git llvm clang cmake base-devel
```

### Installing on macOS

First install [Homebrew](https://brew.sh) if you don't have it, then:
```bash
brew install llvm cmake
```

## Step 1: Download 3BX

Open your terminal and run:
```bash
git clone https://github.com/pexlit/3BX.git
cd 3BX
```

This downloads all the 3BX code to your computer.

## Step 2: Build the Compiler

Now let's build the 3BX compiler. This only takes about a minute:

```bash
mkdir build
cd build
cmake ..
make
```

You should see text scrolling by as it compiles. When it finishes without errors, you're ready to go!

**Tip:** If you see errors, double-check that you installed all the requirements from Step 1.

## Step 3: Write Your First Program

Let's create a simple program! While still in the `build` folder, create a new file called `hello.3bx`:

```
# My first 3BX program!
print "Hello, World!"
```

You can create this file using any text editor you like (Notepad, VS Code, nano, etc.).

## Step 4: Run Your Program

Now for the exciting part - let's run it!

```bash
./3bx hello.3bx
```

You should see:
```
Hello, World!
```

**Congratulations!** You just ran your first 3BX program!

## Step 5: Try Something More

Let's try a slightly bigger program. Create a file called `math.3bx`:

```
# Simple math in 3BX

set a to 10
set b to 25

print "Adding two numbers!"
print a + b
```

Run it:
```bash
./3bx math.3bx
```

You should see:
```
Adding two numbers!
35
```

## Troubleshooting

### "Command not found" error
Make sure you're in the `build` folder and use `./3bx` (with the dot and slash).

### Build errors
Make sure all dependencies are installed. Try running the install command again.

### "File not found" error
Make sure the `.3bx` file is in the same folder where you're running the command.

## What's Next?

Now that you have 3BX working, it's time to learn more:

- [Writing Your First Program](Writing-Your-First-Program.md) - Learn the basics step by step
- [Examples](Examples.md) - See more things you can build
- [Patterns](Patterns.md) - Learn how to create your own commands

Happy coding!
