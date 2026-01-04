# Semantic Tokenization in 3BX

This document describes how the 3BX language server identifies and categorizes tokens for syntax highlighting using the Language Server Protocol's Semantic Tokens capability.

## Overview

Unlike traditional languages that use fixed grammars (Lex/Yacc), 3BX defines its syntax through natural language patterns. Highlighting must therefore be driven by the results of the compilation process, specifically leveraging data from the pattern analysis and resolution stages.

Tokenization follows a "Bottom-Up" approach to handle nested expressions correctly.

## The Semantic Tokens Builder

A specialized `SemanticTokensBuilder` is used to manage tokens on a per-line basis. It provides an `AddToken(start, end, type)` method with the following characteristics:

1.  **Token Cutting**: When a new token is added that overlaps with existing tokens, the builder "cuts" the new token. Existing tokens (usually more specific sub-expressions) always take priority.
2.  **Order of Addition**: Tokens are added from the most specific (inner variables and expressions) to the most general (outer effects and sections).

### Example of Token Cutting:
Existing tokens: `[char 4 to 10: variable]`
```
....vvvvvv.....
```
Adding a new token from char 2 to 13 (e.g., an outer expression):
```
..nnvvvvvvnnn..
```
The result is three distinct tokens:
- Char 2 to 4: `expression`
- Char 4 to 10: `variable`
- Char 10 to 13: `expression`

## Tokenization Lifecycle

Tokenization is a separate process that occurs **after compilation has finished**. It is performed only for requested files (e.g., when a file is opened or edited) and relies on the metadata generated during the analysis and resolution stages.

## Tokenization Algorithm

For a requested file, the LSP server performs the following steps using the metadata from the last successful compilation:

1.  **Priority 1: Variables and Literals**:
    - Identify all variable arguments in pattern matches and definitions.
    - Identify strings, numbers, and comments.
    - Add these tokens first.
2.  **Priority 2: Subexpressions**:
    - Recursively tokenize expression arguments from the most deeply nested to the least nested.
    - Inner expressions cut outer ones.
3.  **Priority 3: Pattern References & Definitions**:
    - Add tokens for pattern references and definitions resolved by the compiler.
    - This covers the literal parts of the patterns.
4.  **Priority 4: Whole Line Fallback**:
    - For parts of the line not yet tokenized:
        - If the line ends with a colon `:`, it is tokenized as a `section`.
        - Otherwise, it is tokenized as an `effect`.
    - This ensures everything (including "hardcoded" words like `get:`, `return`, `if:`) is highlighted according to its semantic role.

## Token Types

3BX uses a streamlined set of token types to distinguish between different semantic roles:

- `effect`: Literal parts of an effect pattern or non-section line.
- `expression`: Literal parts of an expression pattern reference.
- `section`: Literal parts of a section pattern or line ending in a colon.
- `pattern`: Literal parts of a pattern *definition*.
- `function`: Compiler intrinsics (prefixed with `@`).
- `variable`: User-defined variables.
- `string`, `number`, `comment`, `operator`.

## Coordination with VS Code

The client-side `tmLanguage.json` is kept empty to avoid conflicting with the server's semantic data. The VS Code theme provides the color mapping for the types defined by the server.
