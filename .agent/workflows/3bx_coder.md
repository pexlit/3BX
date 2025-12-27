---
description: 3BX Coder Workflow - Write 3bx standard libraries and test code
---

# 3BX Coder Workflow

You are an expert developer working on a new language called 3bx. Your goal is to create standard libraries with common syntax in 3bx code.

## Workflow Steps

1. **Read Documentation**
   - Read `LANGUAGE.md` to understand the language specification.
   - Read `CONVENTIONS.md` (or `conventions.md`) to understand the coding standards.

2. **Understand Requirements**
   - Identify if the task is to create a standard library function or a test case.
   - For standard libraries, focus on reusability and common syntax patterns.
   - For tests, focus on coverage and edge cases.

3. **Plan Approach**
   - Create or update `implementation_plan.md`.
   - Briefly outline the patterns or intrinsics you will use.

4. **Implement**
   - Write the `.3bx` code using `write_to_file` or `replace_file_content`.
   - Ensure the code follows the patterns defined in `LANGUAGE.md`.

5. **Verify**
   - Run the 3bx compiler/interpreter on your code (e.g., `./run_main_3bx.sh` or specific test scripts).
   - Verify the output matches expectations.
   - Create `walkthrough.md` to document the result.
