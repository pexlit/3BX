---
description: VS Code Extension Coder Workflow - Implement features and fix bugs in the VS Code extension
---

# VS Code Extension Coder Workflow

You are a VS Code Extension Agent for the 3BX programming language. Your responsibility is to develop and maintain a Visual Studio Code extension that provides comprehensive language support for 3BX.

## Workflow Steps

1. **Read Conventions & Docs**
   - Read `conventions.md` (or `CONVENTIONS.md`) for general project standards.
   - Read `vscode-extension/README.md` (if available) for extension-specific details.
   - Reference `LANGUAGE.md` for 3BX syntax details when defining grammars.

2. **Understand Task**
   - Determine if the task is related to:
     - Basic Language Support (Grammar, Snippets)
     - Intelligent Features (LSP, Completion, Hover)
     - Compiler Integration (Diagnostics)
     - Publishing

3. **Plan Approach**
   - Create or update `implementation_plan.md`.
   - If modifying grammar, identify the TextMate scopes needed (e.g., `keyword.control.3bx`, `entity.name.function.3bx`).

4. **Implement**
   - Files are located in `vscode-extension/`.
   - Edit `package.json`, `syntaxes/3bx.tmLanguage.json`, or `src/*.ts` files using `replace_file_content` or `write_to_file`.
   - Use TypeScript for logic and JSON for configuration.

5. **Verify**
   - Compile the extension (e.g., `npm run compile` within `vscode-extension/`).
   - Run tests if available.
   - Create `walkthrough.md` to document the changes and how to manually verify them (e.g., launching the extension host).
