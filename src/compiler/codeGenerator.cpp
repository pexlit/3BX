#include "compiler/codeGenerator.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <sstream>

namespace tbx {

// Trim whitespace from both ends of a string
static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

// ============================================================================
// SectionCodeGenerator Implementation
// ============================================================================

SectionCodeGenerator::SectionCodeGenerator(const std::string& moduleName) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(moduleName, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
}

bool SectionCodeGenerator::generate(SectionPatternResolver& resolver, Section* root) {
    if (!root) {
        diagnostics_.emplace_back("Cannot generate code from null section");
        return false;
    }

    // Clear previous state
    codegenPatterns_.clear();
    patternToCodegen_.clear();
    namedValues_.clear();
    diagnostics_.clear();

    // Run type inference internally
    runTypeInference(resolver);

    // Generate external declarations (printf, etc.)
    generateExternalDeclarations();

    // Two-pass function generation:
    // Pass 1: Declare all functions (so they can call each other)
    for (auto& codegenPattern : codegenPatterns_) {
        declarePatternFunction(*codegenPattern);
    }

    // Pass 2: Generate function bodies
    for (auto& codegenPattern : codegenPatterns_) {
        generatePatternFunctionBody(*codegenPattern);
    }

    // Generate main function from top-level code
    generateMain(root, resolver);

    // Verify the module
    std::string verifyError;
    llvm::raw_string_ostream verifyStream(verifyError);
    if (llvm::verifyModule(*module_, &verifyStream)) {
        diagnostics_.emplace_back("Module verification failed: " + verifyError);
        return false;
    }

    bool hasError = false;
    for (const auto& diag : diagnostics_) {
        if (diag.severity == DiagnosticSeverity::Error) {
            hasError = true;
            break;
        }
    }
    return !hasError;
}

bool SectionCodeGenerator::generate(const TypeInference& typeInference, SectionPatternResolver& resolver, Section* root) {
    if (!root) {
        diagnostics_.emplace_back("Cannot generate code from null section");
        return false;
    }

    // Clear previous state
    codegenPatterns_.clear();
    patternToCodegen_.clear();
    namedValues_.clear();
    diagnostics_.clear();

    // Use provided type inference results
    for (const auto& typedPattern : typeInference.typedPatterns()) {
        auto codegen = std::make_unique<CodegenPattern>();
        codegen->typedPattern = typedPattern.get();

        // Generate function name
        std::string baseName;
        if (typedPattern->pattern) {
            switch (typedPattern->pattern->type) {
                case PatternType::Effect:
                    baseName = "effect_";
                    break;
                case PatternType::Expression:
                    baseName = "expr_";
                    break;
                case PatternType::Section:
                    baseName = "section_";
                    break;
            }

            std::string cleanName;
            for (char c : typedPattern->pattern->pattern) {
                if (std::isalnum(c)) {
                    cleanName += c;
                } else if (c == ' ' && !cleanName.empty() && cleanName.back() != '_') {
                    cleanName += '_';
                }
            }
            codegen->functionName = baseName + cleanName;

            // Collect parameter names in order
            for (const auto& var : typedPattern->pattern->variables) {
                codegen->parameterNames.push_back(var);
            }

            patternToCodegen_[typedPattern->pattern] = codegen.get();
        }

        codegenPatterns_.push_back(std::move(codegen));
    }

    // Generate external declarations
    generateExternalDeclarations();

    // Two-pass function generation:
    // Pass 1: Declare all functions (so they can call each other)
    for (auto& codegenPattern : codegenPatterns_) {
        declarePatternFunction(*codegenPattern);
    }

    // Pass 2: Generate function bodies
    for (auto& codegenPattern : codegenPatterns_) {
        generatePatternFunctionBody(*codegenPattern);
    }

    // Generate main
    generateMain(root, resolver);

    // Verify module
    std::string verifyError;
    llvm::raw_string_ostream verifyStream(verifyError);
    if (llvm::verifyModule(*module_, &verifyStream)) {
        diagnostics_.emplace_back("Module verification failed: " + verifyError);
        return false;
    }

    bool hasError = false;
    for (const auto& diag : diagnostics_) {
        if (diag.severity == DiagnosticSeverity::Error) {
            hasError = true;
            break;
        }
    }
    return !hasError;
}

bool SectionCodeGenerator::writeIr(const std::string& filename) {
    std::error_code ec;
    llvm::raw_fd_ostream out(filename, ec, llvm::sys::fs::OF_None);
    if (ec) {
        diagnostics_.emplace_back("Cannot open file: " + filename);
        return false;
    }
    module_->print(out, nullptr);
    return true;
}

void SectionCodeGenerator::printIr() {
    module_->print(llvm::outs(), nullptr);
}

// ============================================================================
// Type Inference (Internal)
// ============================================================================

void SectionCodeGenerator::runTypeInference(SectionPatternResolver& resolver) {
    typeInference_ = std::make_unique<TypeInference>();
    typeInference_->infer(resolver);

    // Build codegen patterns from typed patterns
    for (const auto& typedPattern : typeInference_->typedPatterns()) {
        auto codegen = std::make_unique<CodegenPattern>();
        codegen->typedPattern = typedPattern.get();

        // Generate function name
        std::string baseName;
        if (typedPattern->pattern) {
            switch (typedPattern->pattern->type) {
                case PatternType::Effect:
                    baseName = "effect_";
                    break;
                case PatternType::Expression:
                    baseName = "expr_";
                    break;
                case PatternType::Section:
                    baseName = "section_";
                    break;
            }

            std::string cleanName;
            for (char c : typedPattern->pattern->pattern) {
                if (std::isalnum(c)) {
                    cleanName += c;
                } else if (c == ' ' && !cleanName.empty() && cleanName.back() != '_') {
                    cleanName += '_';
                }
            }
            codegen->functionName = baseName + cleanName;

            // Collect parameter names in order
            for (const auto& var : typedPattern->pattern->variables) {
                codegen->parameterNames.push_back(var);
            }

            patternToCodegen_[typedPattern->pattern] = codegen.get();
        }

        codegenPatterns_.push_back(std::move(codegen));
    }
}

llvm::Type* SectionCodeGenerator::typeToLlvm(InferredType type) {
    switch (type) {
        case InferredType::I64:
            return llvm::Type::getInt64Ty(*context_);
        case InferredType::F64:
            return llvm::Type::getDoubleTy(*context_);
        case InferredType::String:
            return llvm::PointerType::get(llvm::Type::getInt8Ty(*context_), 0);
        case InferredType::I1:
            return llvm::Type::getInt1Ty(*context_);
        case InferredType::Void:
            return llvm::Type::getVoidTy(*context_);
        case InferredType::Unknown:
            return llvm::Type::getInt64Ty(*context_);  // Default to i64
    }
    return llvm::Type::getInt64Ty(*context_);
}

InferredType SectionCodeGenerator::inferReturnTypeFromPattern(const ResolvedPattern* pattern) {
    if (!pattern) return InferredType::Void;

    switch (pattern->type) {
        case PatternType::Effect:
            return InferredType::Void;
        case PatternType::Expression:
            return InferredType::I64;  // Default expressions return i64 (including booleans)
        case PatternType::Section:
            return InferredType::Void;
    }
    return InferredType::Void;
}

// ============================================================================
// Code Generation
// ============================================================================

void SectionCodeGenerator::generateExternalDeclarations() {
    // Declare printf
    llvm::FunctionType* printfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context_),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(*context_), 0)},
        true  // vararg
    );
    printfFunc_ = module_->getOrInsertFunction("printf", printfType);

    // Note: format strings will be created lazily when needed
    // They require an insertion point, which is set when generating main()
    fmtInt_ = nullptr;
    fmtFloat_ = nullptr;
    fmtStr_ = nullptr;
}

void SectionCodeGenerator::declarePatternFunction(CodegenPattern& codegenPattern) {
    TypedPattern* typed = codegenPattern.typedPattern;
    if (!typed || !typed->pattern || !typed->pattern->body) {
        return;
    }

    ResolvedPattern* pattern = typed->pattern;

    // Skip single-word patterns that are just section markers (like "execute:", "get:")
    if (pattern->isSingleWord() && pattern->body->lines.empty()) {
        return;
    }

    // Create parameter types
    std::vector<llvm::Type*> paramTypes;
    for (const auto& varName : codegenPattern.parameterNames) {
        auto it = typed->parameterTypes.find(varName);
        InferredType paramType = (it != typed->parameterTypes.end()) ? it->second : InferredType::I64;
        paramTypes.push_back(typeToLlvm(paramType));
    }

    // Create function type
    llvm::Type* returnType = typeToLlvm(typed->returnType);
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType,
        paramTypes,
        false
    );

    // Create function (declaration only - no body yet)
    llvm::Function* function = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        codegenPattern.functionName,
        module_.get()
    );
    codegenPattern.llvmFunction = function;

    // Set parameter names
    size_t idx = 0;
    for (auto& arg : function->args()) {
        if (idx < codegenPattern.parameterNames.size()) {
            arg.setName(codegenPattern.parameterNames[idx]);
            idx++;
        }
    }
}

void SectionCodeGenerator::generatePatternFunctionBody(CodegenPattern& codegenPattern) {
    TypedPattern* typed = codegenPattern.typedPattern;
    if (!typed || !typed->pattern || !typed->pattern->body) {
        return;
    }

    llvm::Function* function = codegenPattern.llvmFunction;
    if (!function) {
        return;  // Function wasn't declared (e.g., skipped section marker)
    }

    ResolvedPattern* pattern = typed->pattern;

    // Create entry block
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", function);
    builder_->SetInsertPoint(entry);

    // Create allocas for parameters and add to namedValues_
    currentFunction_ = function;
    namedValues_.clear();

    size_t idx = 0;
    for (auto& arg : function->args()) {
        const std::string& name = codegenPattern.parameterNames[idx];
        llvm::AllocaInst* alloca = createEntryAlloca(function, name, arg.getType());
        builder_->CreateStore(&arg, alloca);
        namedValues_[name] = alloca;
        idx++;
    }

    // Generate code for pattern body
    // First, look for the relevant section (like "execute:" or "get:")
    Section* bodySection = nullptr;
    for (const auto& line : pattern->body->lines) {
        std::string lineText = trim(line.text);
        // Remove trailing colon if present
        if (!lineText.empty() && lineText.back() == ':') {
            lineText.pop_back();
        }

        // Check for known section types
        if (lineText == "execute" || lineText == "get" || lineText == "check") {
            if (line.childSection) {
                bodySection = line.childSection.get();
                break;
            }
        }
    }

    llvm::Value* result = nullptr;
    if (bodySection) {
        // Generate code for each line in the body section
        for (const auto& line : bodySection->lines) {
            result = generateBodyLine(line.text);
        }
    }

    // Add appropriate return
    if (typed->returnType == InferredType::Void) {
        builder_->CreateRetVoid();
    } else {
        llvm::Type* expectedRetType = typeToLlvm(typed->returnType);
        bool hasValidResult = result && !result->getType()->isVoidTy();

        if (hasValidResult) {
            // Check if result type matches expected return type
            if (result->getType() != expectedRetType) {
                // Handle i1 to i64 conversion for comparison results
                if (result->getType()->isIntegerTy(1) && expectedRetType->isIntegerTy(64)) {
                    result = builder_->CreateZExt(result, expectedRetType, "zext_cmp");
                }
                // Handle i64 to i1 conversion (truncate)
                else if (result->getType()->isIntegerTy(64) && expectedRetType->isIntegerTy(1)) {
                    result = builder_->CreateTrunc(result, expectedRetType, "trunc_bool");
                }
                // Handle pointer to integer (invalid - use default)
                else if (result->getType()->isPointerTy() && expectedRetType->isIntegerTy()) {
                    hasValidResult = false;
                }
            }
        }

        if (hasValidResult) {
            builder_->CreateRet(result);
        } else {
            // Default return value
            builder_->CreateRet(llvm::ConstantInt::get(expectedRetType, 0));
        }
    }

    currentFunction_ = nullptr;
}

void SectionCodeGenerator::generateMain(Section* root, SectionPatternResolver& resolver) {
    // Create main function
    llvm::FunctionType* mainType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context_),
        {},
        false
    );

    llvm::Function* mainFunc = llvm::Function::Create(
        mainType,
        llvm::Function::ExternalLinkage,
        "main",
        module_.get()
    );

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", mainFunc);
    builder_->SetInsertPoint(entry);
    currentFunction_ = mainFunc;
    namedValues_.clear();

    // Generate code for top-level pattern references
    for (const auto& line : root->lines) {
        // Skip pattern definitions (they become functions)
        if (line.isPatternDefinition) {
            continue;
        }

        // Try to find a matching pattern for this line
        generateCodeLine(const_cast<CodeLine*>(&line), resolver);
    }

    // Return 0 from main
    builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
    currentFunction_ = nullptr;
}

llvm::Value* SectionCodeGenerator::generateCodeLine(CodeLine* line, SectionPatternResolver& resolver) {
    if (!line) return nullptr;

    std::string text = trim(line->text);
    if (text.empty()) return nullptr;

    // Check if this is a direct intrinsic call
    if (text.find("@intrinsic(") != std::string::npos) {
        return generateIntrinsic(text, {});
    }

    // Parse the line into words (used by pattern matching and fallback)
    // Handle quoted strings as single tokens
    std::vector<std::string> lineWords;
    {
        size_t pos = 0;
        while (pos < text.size()) {
            // Skip whitespace
            while (pos < text.size() && std::isspace(text[pos])) {
                pos++;
            }
            if (pos >= text.size()) break;

            std::string token;
            if (text[pos] == '"' || text[pos] == '\'') {
                // Quoted string - capture until closing quote
                char quote = text[pos];
                token += text[pos++];
                while (pos < text.size() && text[pos] != quote) {
                    token += text[pos++];
                }
                if (pos < text.size()) {
                    token += text[pos++];  // Include closing quote
                }
            } else {
                // Regular word - capture until whitespace
                while (pos < text.size() && !std::isspace(text[pos])) {
                    token += text[pos++];
                }
            }
            if (!token.empty()) {
                lineWords.push_back(token);
            }
        }
    }

    // Try to parse as a pattern call by matching the line text
    for (auto& codegenPattern : codegenPatterns_) {
        if (!codegenPattern->llvmFunction) continue;

        TypedPattern* typed = codegenPattern->typedPattern;
        if (!typed || !typed->pattern) continue;

        ResolvedPattern* pattern = typed->pattern;

        // Special case: check if the line text exactly matches the pattern's original text
        if (pattern->originalText == text) {
            llvm::FunctionType* funcType = codegenPattern->llvmFunction->getFunctionType();
            if (funcType->getNumParams() == 0) {
                return builder_->CreateCall(codegenPattern->llvmFunction, {});
            } else {
                std::vector<llvm::Value*> defaultArgs;
                for (unsigned i = 0; i < funcType->getNumParams(); i++) {
                    llvm::Type* paramType = funcType->getParamType(i);
                    if (paramType->isIntegerTy()) {
                        defaultArgs.push_back(llvm::ConstantInt::get(paramType, 0));
                    } else if (paramType->isDoubleTy()) {
                        defaultArgs.push_back(llvm::ConstantFP::get(paramType, 0.0));
                    } else {
                        defaultArgs.push_back(llvm::Constant::getNullValue(paramType));
                    }
                }
                return builder_->CreateCall(codegenPattern->llvmFunction, defaultArgs);
            }
        }

        // Parse pattern into words
        std::vector<std::string> patternWords;
        std::string word;
        std::istringstream pss(pattern->pattern);
        while (pss >> word) {
            patternWords.push_back(word);
        }

        // Quick check: same number of words
        if (lineWords.size() != patternWords.size()) continue;

        // Try to match
        bool matches = true;
        std::vector<llvm::Value*> args;

        for (size_t i = 0; i < patternWords.size(); i++) {
            if (patternWords[i] == "$") {
                // Variable slot - extract argument
                const std::string& argStr = lineWords[i];

                // Use generateExpression to properly handle literals, variables, etc.
                llvm::Value* argVal = generateExpression(argStr, {});
                if (argVal) {
                    args.push_back(argVal);
                } else {
                    // Unknown identifier - pass as string constant (for variable names etc.)
                    args.push_back(builder_->CreateGlobalStringPtr(argStr));
                }
            } else {
                // Literal word - must match exactly
                if (patternWords[i] != lineWords[i]) {
                    matches = false;
                    break;
                }
            }
        }

        if (matches && args.size() == codegenPattern->parameterNames.size()) {
            // Check type compatibility before calling
            llvm::FunctionType* funcType = codegenPattern->llvmFunction->getFunctionType();
            bool typesCompatible = true;

            for (size_t i = 0; i < args.size(); i++) {
                llvm::Type* expectedType = funcType->getParamType(i);
                llvm::Type* actualType = args[i]->getType();

                if (actualType != expectedType) {
                    // Try type conversions
                    if (actualType->isIntegerTy() && expectedType->isIntegerTy()) {
                        // Integer width conversion
                        if (actualType->getIntegerBitWidth() < expectedType->getIntegerBitWidth()) {
                            args[i] = builder_->CreateZExt(args[i], expectedType, "zext");
                        } else {
                            args[i] = builder_->CreateTrunc(args[i], expectedType, "trunc");
                        }
                    } else if (actualType->isPointerTy() && expectedType->isIntegerTy()) {
                        typesCompatible = false;
                        break;
                    } else if (actualType->isIntegerTy() && expectedType->isPointerTy()) {
                        typesCompatible = false;
                        break;
                    } else {
                        typesCompatible = false;
                        break;
                    }
                }
            }

            if (typesCompatible) {
                // Call the pattern function
                return builder_->CreateCall(codegenPattern->llvmFunction, args);
            }
        }
    }

    // Fallback: Direct handling for common patterns
    for (auto& codegenPattern : codegenPatterns_) {
        if (!codegenPattern->llvmFunction) continue;

        TypedPattern* typed = codegenPattern->typedPattern;
        if (!typed || !typed->pattern || !typed->pattern->body) continue;

        ResolvedPattern* pattern = typed->pattern;

        std::vector<std::string> patternWords;
        std::istringstream pss(pattern->pattern);
        std::string word;
        while (pss >> word) {
            patternWords.push_back(word);
        }

        if (lineWords.size() != patternWords.size()) continue;

        bool matches = true;
        std::vector<std::string> argStrings;

        for (size_t i = 0; i < patternWords.size() && matches; i++) {
            if (patternWords[i] == "$") {
                argStrings.push_back(lineWords[i]);
            } else if (patternWords[i] != lineWords[i]) {
                matches = false;
            }
        }

        if (!matches) continue;

        for (const auto& bodyLine : pattern->body->lines) {
            std::string lineText = trim(bodyLine.text);
            if (!lineText.empty() && lineText.back() == ':') {
                lineText.pop_back();
            }

            if ((lineText == "execute" || lineText == "get") && bodyLine.childSection) {
                for (const auto& childLine : bodyLine.childSection->lines) {
                    std::string childText = trim(childLine.text);

                    if (childText.find("@intrinsic(") != std::string::npos) {
                        std::string expanded = childText;
                        for (size_t pi = 0; pi < pattern->variables.size() && pi < argStrings.size(); pi++) {
                            const std::string& paramName = pattern->variables[pi];
                            const std::string& argValue = argStrings[pi];

                            size_t pos = 0;
                            while ((pos = expanded.find(paramName, pos)) != std::string::npos) {
                                bool startOk = (pos == 0 || !std::isalnum(expanded[pos-1]));
                                bool endOk = (pos + paramName.size() >= expanded.size() ||
                                               !std::isalnum(expanded[pos + paramName.size()]));
                                if (startOk && endOk) {
                                    expanded.replace(pos, paramName.size(), argValue);
                                    pos += argValue.size();
                                } else {
                                    pos++;
                                }
                            }
                        }

                        llvm::Value* res = generateIntrinsic(expanded, {});
                        if (res || expanded.find("\"print\"") != std::string::npos ||
                            expanded.find("\"store\"") != std::string::npos) {
                            return res;
                        }
                    }
                }
            }
        }
    }

    return nullptr;
}

llvm::Value* SectionCodeGenerator::generateBodyLine(const std::string& text) {
    std::string trimmed = trim(text);
    if (trimmed.empty()) return nullptr;

    if (trimmed.find("@intrinsic(") != std::string::npos) {
        std::string check = trimmed;
        if (check.substr(0, 7) == "return ") {
            check = trim(check.substr(7));
        }
        if (check.find("@intrinsic(") == 0) {
            return generateIntrinsic(trimmed, {});
        }
    }

    for (auto& codegenPattern : codegenPatterns_) {
        if (!codegenPattern->llvmFunction) continue;

        TypedPattern* typed = codegenPattern->typedPattern;
        if (!typed || !typed->pattern) continue;

        ResolvedPattern* pattern = typed->pattern;

        if (pattern->originalText == trimmed) {
            llvm::FunctionType* funcType = codegenPattern->llvmFunction->getFunctionType();
            if (funcType->getNumParams() == 0) {
                return builder_->CreateCall(codegenPattern->llvmFunction, {});
            }
        }
    }

    for (auto& codegenPattern : codegenPatterns_) {
        if (!codegenPattern->llvmFunction) continue;

        TypedPattern* typed = codegenPattern->typedPattern;
        if (!typed || !typed->pattern) continue;

        ResolvedPattern* pattern = typed->pattern;

        if (pattern->pattern == "set $ to $" || pattern->pattern == "return $") {
            continue;
        }

        std::vector<std::string> patternWords;
        std::istringstream pss(pattern->pattern);
        std::string word;
        while (pss >> word) {
            patternWords.push_back(word);
        }

        std::vector<std::string> extractedArgs;
        bool matches = true;
        size_t textPos = 0;

        for (size_t pi = 0; pi < patternWords.size() && matches; pi++) {
            while (textPos < trimmed.size() && std::isspace(trimmed[textPos])) {
                textPos++;
            }

            if (patternWords[pi] == "$") {
                std::string nextLiteral;
                if (pi + 1 < patternWords.size() && patternWords[pi + 1] != "$") {
                    nextLiteral = patternWords[pi + 1];
                }

                std::string arg;
                if (nextLiteral.empty()) {
                    arg = extractArgument(trimmed, textPos);
                } else {
                    arg = extractArgumentUntil(trimmed, textPos, nextLiteral);
                }

                if (arg.empty()) {
                    matches = false;
                } else {
                    extractedArgs.push_back(arg);
                }
            } else {
                std::string literal = patternWords[pi];
                if (trimmed.compare(textPos, literal.size(), literal) == 0) {
                    size_t endPos = textPos + literal.size();
                    if (endPos == trimmed.size() || std::isspace(trimmed[endPos])) {
                        textPos = endPos;
                    } else {
                        matches = false;
                    }
                } else {
                    matches = false;
                }
            }
        }

        while (textPos < trimmed.size() && std::isspace(trimmed[textPos])) {
            textPos++;
        }
        if (textPos != trimmed.size()) {
            matches = false;
        }

        if (matches && extractedArgs.size() == codegenPattern->parameterNames.size()) {
            std::vector<llvm::Value*> args;
            bool typesCompatible = true;

            llvm::FunctionType* funcType = codegenPattern->llvmFunction->getFunctionType();

            for (size_t i = 0; i < extractedArgs.size(); i++) {
                const auto& argStr = extractedArgs[i];
                llvm::Value* argVal = generateExpression(argStr, {});
                if (!argVal) {
                    argVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
                }

                llvm::Type* expectedType = funcType->getParamType(i);
                if (argVal->getType() != expectedType) {
                    if (argVal->getType()->isIntegerTy() && expectedType->isIntegerTy()) {
                        if (argVal->getType()->getIntegerBitWidth() < expectedType->getIntegerBitWidth()) {
                            argVal = builder_->CreateZExt(argVal, expectedType, "zext");
                        } else {
                            argVal = builder_->CreateTrunc(argVal, expectedType, "trunc");
                        }
                    } else if (argVal->getType()->isPointerTy() && expectedType->isIntegerTy()) {
                        typesCompatible = false;
                        break;
                    } else {
                        typesCompatible = false;
                        break;
                    }
                }

                args.push_back(argVal);
            }

            if (typesCompatible) {
                return builder_->CreateCall(codegenPattern->llvmFunction, args);
            }
        }
    }

    if (trimmed.size() > 6 && trimmed.substr(0, 6) == "print ") {
        std::string argStr = trim(trimmed.substr(6));
        llvm::Value* val = generateExpression(argStr, {});
        if (val) {
            if (!fmtInt_) {
                fmtInt_ = builder_->CreateGlobalStringPtr("%lld\n", ".str.int");
            }
            if (!fmtStr_) {
                fmtStr_ = builder_->CreateGlobalStringPtr("%s\n", ".str.str");
            }

            llvm::Value* fmt;
            if (val->getType()->isIntegerTy()) {
                fmt = fmtInt_;
            } else {
                fmt = fmtStr_;
            }
            builder_->CreateCall(printfFunc_, {fmt, val});
            return nullptr;
        }
    }

    if (trimmed.size() > 4 && trimmed.substr(0, 4) == "set ") {
        size_t toPos = trimmed.find(" to ");
        if (toPos != std::string::npos) {
            std::string varName = trim(trimmed.substr(4, toPos - 4));
            std::string valStr = trim(trimmed.substr(toPos + 4));
            llvm::Value* val = generateExpression(valStr, {});
            if (val) {
                auto it = namedValues_.find(varName);
                if (it != namedValues_.end()) {
                    builder_->CreateStore(val, it->second);
                } else {
                    llvm::AllocaInst* alloca = createEntryAlloca(currentFunction_, varName, val->getType());
                    builder_->CreateStore(val, alloca);
                    namedValues_[varName] = alloca;
                }
                return nullptr;
            }
        }
    }

    return nullptr;
}

std::string SectionCodeGenerator::extractArgument(const std::string& text, size_t& pos) {
    if (pos >= text.size()) return "";

    while (pos < text.size() && std::isspace(text[pos])) {
        pos++;
    }

    if (pos >= text.size()) return "";

    size_t start = pos;

    if (text[pos] == '"' || text[pos] == '\'') {
        char quote = text[pos];
        pos++;
        while (pos < text.size() && text[pos] != quote) {
            pos++;
        }
        if (pos < text.size()) pos++;
        return text.substr(start, pos - start);
    }

    if (text.substr(pos, 10) == "@intrinsic") {
        pos += 10;
        if (pos < text.size() && text[pos] == '(') {
            int depth = 1;
            pos++;
            while (pos < text.size() && depth > 0) {
                if (text[pos] == '(') depth++;
                else if (text[pos] == ')') depth--;
                pos++;
            }
            return text.substr(start, pos - start);
        }
    }

    while (pos < text.size() && !std::isspace(text[pos])) {
        pos++;
    }

    return text.substr(start, pos - start);
}

std::string SectionCodeGenerator::extractArgumentUntil(const std::string& text, size_t& pos, const std::string& untilWord) {
    if (pos >= text.size()) return "";

    while (pos < text.size() && std::isspace(text[pos])) {
        pos++;
    }

    if (pos >= text.size()) return "";

    size_t start = pos;

    if (text[pos] == '"' || text[pos] == '\'') {
        char quote = text[pos];
        pos++;
        while (pos < text.size() && text[pos] != quote) {
            pos++;
        }
        if (pos < text.size()) pos++;
        return text.substr(start, pos - start);
    }

    if (text.substr(pos, 10) == "@intrinsic") {
        pos += 10;
        if (pos < text.size() && text[pos] == '(') {
            int depth = 1;
            pos++;
            while (pos < text.size() && depth > 0) {
                if (text[pos] == '(') depth++;
                else if (text[pos] == ')') depth--;
                pos++;
            }
            return text.substr(start, pos - start);
        }
    }

    while (pos < text.size()) {
        size_t wsStart = pos;
        while (pos < text.size() && std::isspace(text[pos])) {
            pos++;
        }

        if (text.compare(pos, untilWord.size(), untilWord) == 0) {
            size_t endCheck = pos + untilWord.size();
            if (endCheck == text.size() || std::isspace(text[endCheck])) {
                pos = wsStart;
                return trim(text.substr(start, wsStart - start));
            }
        }

        if (pos < text.size() && !std::isspace(text[pos])) {
            if (text[pos] == '"' || text[pos] == '\'') {
                char quote = text[pos];
                pos++;
                while (pos < text.size() && text[pos] != quote) {
                    pos++;
                }
                if (pos < text.size()) pos++;
            }
            else if (text.substr(pos, 10) == "@intrinsic") {
                pos += 10;
                if (pos < text.size() && text[pos] == '(') {
                    int depth = 1;
                    pos++;
                    while (pos < text.size() && depth > 0) {
                        if (text[pos] == '(') depth++;
                        else if (text[pos] == ')') depth--;
                        pos++;
                    }
                }
            }
            else {
                while (pos < text.size() && !std::isspace(text[pos])) {
                    pos++;
                }
            }
        }
    }

    return trim(text.substr(start));
}

llvm::Value* SectionCodeGenerator::generatePatternCall(PatternMatch* match) {
    if (!match || !match->pattern) return nullptr;

    auto it = patternToCodegen_.find(match->pattern);
    if (it == patternToCodegen_.end()) return nullptr;

    CodegenPattern* codegenPattern = it->second;
    if (!codegenPattern->llvmFunction) return nullptr;

    std::vector<llvm::Value*> args;
    for (const auto& paramName : codegenPattern->parameterNames) {
        auto argIt = match->arguments.find(paramName);
        if (argIt != match->arguments.end()) {
            const ResolvedValue& val = argIt->second.value;

            if (std::holds_alternative<int64_t>(val)) {
                args.push_back(llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(*context_),
                    std::get<int64_t>(val),
                    true));
            } else if (std::holds_alternative<double>(val)) {
                args.push_back(llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(*context_),
                    std::get<double>(val)));
            } else if (std::holds_alternative<std::string>(val)) {
                const std::string& str = std::get<std::string>(val);

                bool isInt = !str.empty();
                for (size_t i = 0; i < str.size(); i++) {
                    char c = str[i];
                    if (c == '-' && i == 0) continue;
                    if (!std::isdigit(c)) { isInt = false; break; }
                }

                if (isInt) {
                    args.push_back(llvm::ConstantInt::get(
                        llvm::Type::getInt64Ty(*context_),
                        std::stoll(str),
                        true));
                } else {
                    auto varIt = namedValues_.find(str);
                    if (varIt != namedValues_.end()) {
                        args.push_back(builder_->CreateLoad(
                            varIt->second->getAllocatedType(),
                            varIt->second,
                            str));
                    } else {
                        args.push_back(llvm::ConstantInt::get(
                            llvm::Type::getInt64Ty(*context_), 0));
                    }
                }
            } else {
                args.push_back(llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(*context_), 0));
            }
        } else {
            args.push_back(llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(*context_), 0));
        }
    }

    return builder_->CreateCall(codegenPattern->llvmFunction, args);
}

llvm::Value* SectionCodeGenerator::generateIntrinsic(
    const std::string& text,
    const std::unordered_map<std::string, llvm::Value*>& localVars
) {
    std::string name;
    std::vector<std::string> args;

    if (!parseIntrinsic(text, name, args)) {
        return nullptr;
    }

    if (name == "add" || name == "sub" || name == "mul" || name == "div") {
        if (args.size() < 2) return nullptr;

        llvm::Value* left = generateExpression(args[0], localVars);
        llvm::Value* right = generateExpression(args[1], localVars);
        if (!left || !right) return nullptr;

        if (name == "add") {
            return builder_->CreateAdd(left, right, "addtmp");
        } else if (name == "sub") {
            return builder_->CreateSub(left, right, "subtmp");
        } else if (name == "mul") {
            return builder_->CreateMul(left, right, "multmp");
        } else if (name == "div") {
            return builder_->CreateSDiv(left, right, "divtmp");
        }
    }
    else if (name == "print") {
        if (args.size() < 1) return nullptr;

        llvm::Value* val = generateExpression(args[0], localVars);
        if (!val) return nullptr;

        if (!fmtInt_) {
            fmtInt_ = builder_->CreateGlobalStringPtr("%lld\n", ".str.int");
        }
        if (!fmtFloat_) {
            fmtFloat_ = builder_->CreateGlobalStringPtr("%f\n", ".str.float");
        }
        if (!fmtStr_) {
            fmtStr_ = builder_->CreateGlobalStringPtr("%s\n", ".str.str");
        }

        llvm::Value* fmt;
        if (val->getType()->isIntegerTy()) {
            fmt = fmtInt_;
        } else if (val->getType()->isFloatingPointTy()) {
            fmt = fmtFloat_;
        } else {
            fmt = fmtStr_;
        }

        builder_->CreateCall(printfFunc_, {fmt, val});
        return nullptr;
    }
    else if (name == "store") {
        if (args.size() < 2) return nullptr;

        std::string varName = trim(args[0]);
        llvm::Value* val = generateExpression(args[1], localVars);
        if (!val) return nullptr;

        auto it = namedValues_.find(varName);
        if (it != namedValues_.end()) {
            builder_->CreateStore(val, it->second);
        } else {
            // Create new variable
            llvm::AllocaInst* alloca = createEntryAlloca(currentFunction_, varName, val->getType());
            builder_->CreateStore(val, alloca);
            namedValues_[varName] = alloca;
        }
        return nullptr;
    }
    else if (name == "load") {
        if (args.size() < 1) return nullptr;

        std::string varName = trim(args[0]);
        auto it = namedValues_.find(varName);
        if (it != namedValues_.end()) {
            return builder_->CreateLoad(it->second->getAllocatedType(), it->second, varName);
        }
        return nullptr;
    }
    else if (name == "return") {
        if (args.empty()) {
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
        }
        return generateExpression(args[0], localVars);
    }
    else if (name == "cmp_lt") {
        if (args.size() < 2) return nullptr;
        llvm::Value* left = generateExpression(args[0], localVars);
        llvm::Value* right = generateExpression(args[1], localVars);
        if (!left || !right) return nullptr;
        return builder_->CreateICmpSLT(left, right, "cmptmp");
    }
    else if (name == "cmp_gt") {
        if (args.size() < 2) return nullptr;
        llvm::Value* left = generateExpression(args[0], localVars);
        llvm::Value* right = generateExpression(args[1], localVars);
        if (!left || !right) return nullptr;
        return builder_->CreateICmpSGT(left, right, "cmptmp");
    }
    else if (name == "cmp_eq") {
        if (args.size() < 2) return nullptr;
        llvm::Value* left = generateExpression(args[0], localVars);
        llvm::Value* right = generateExpression(args[1], localVars);
        if (!left || !right) return nullptr;
        return builder_->CreateICmpEQ(left, right, "eqtmp");
    }
    else if (name == "cmp_neq") {
        if (args.size() < 2) return nullptr;
        llvm::Value* left = generateExpression(args[0], localVars);
        llvm::Value* right = generateExpression(args[1], localVars);
        if (!left || !right) return nullptr;
        return builder_->CreateICmpNE(left, right, "netmp");
    }
    else if (name == "cmp_lte") {
        if (args.size() < 2) return nullptr;
        llvm::Value* left = generateExpression(args[0], localVars);
        llvm::Value* right = generateExpression(args[1], localVars);
        if (!left || !right) return nullptr;
        return builder_->CreateICmpSLE(left, right, "cmptmp");
    }
    else if (name == "cmp_gte") {
        if (args.size() < 2) return nullptr;
        llvm::Value* left = generateExpression(args[0], localVars);
        llvm::Value* right = generateExpression(args[1], localVars);
        if (!left || !right) return nullptr;
        return builder_->CreateICmpSGE(left, right, "cmptmp");
    }

    return nullptr;
}

bool SectionCodeGenerator::parseIntrinsic(
    const std::string& text,
    std::string& name,
    std::vector<std::string>& args
) {
    std::string trimmed = trim(text);

    size_t returnPos = trimmed.find("return ");
    if (returnPos == 0) {
        trimmed = trim(trimmed.substr(7));
    }

    size_t start = trimmed.find("@intrinsic(");
    if (start == std::string::npos) {
        return false;
    }

    size_t openParen = start + 10;
    int parenDepth = 1;
    size_t closeParen = openParen + 1;

    while (closeParen < trimmed.size() && parenDepth > 0) {
        if (trimmed[closeParen] == '(') parenDepth++;
        else if (trimmed[closeParen] == ')') parenDepth--;
        closeParen++;
    }
    closeParen--;

    if (parenDepth != 0) {
        return false;
    }

    std::string content = trimmed.substr(openParen + 1, closeParen - openParen - 1);

    args.clear();
    std::string currentArg;
    int depth = 0;
    bool inQuotes = false;
    char quoteChar = '\0';

    for (size_t i = 0; i < content.size(); i++) {
        char c = content[i];

        if (inQuotes) {
            currentArg += c;
            if (c == quoteChar) {
                inQuotes = false;
            }
        } else if (c == '"' || c == '\'') {
            inQuotes = true;
            quoteChar = c;
            currentArg += c;
        } else if (c == '(') {
            depth++;
            currentArg += c;
        } else if (c == ')') {
            depth--;
            currentArg += c;
        } else if (c == ',' && depth == 0) {
            args.push_back(trim(currentArg));
            currentArg.clear();
        } else {
            currentArg += c;
        }
    }
    if (!currentArg.empty()) {
        args.push_back(trim(currentArg));
    }

    if (args.empty()) {
        return false;
    }

    name = args[0];
    if (name.size() >= 2 && (name.front() == '"' || name.front() == '\'')) {
        name = name.substr(1, name.size() - 2);
    }

    args.erase(args.begin());

    return true;
}

llvm::Value* SectionCodeGenerator::generateExpression(
    const std::string& arg,
    const std::unordered_map<std::string, llvm::Value*>& localVars
) {
    std::string trimmed = trim(arg);
    if (trimmed.empty()) return nullptr;

    if (trimmed.find("@intrinsic(") != std::string::npos) {
        return generateIntrinsic(trimmed, localVars);
    }

    bool isInt = !trimmed.empty();
    for (size_t i = 0; i < trimmed.size(); i++) {
        char c = trimmed[i];
        if (c == '-' && i == 0) continue;
        if (!std::isdigit(c)) { isInt = false; break; }
    }

    if (isInt) {
        int64_t val = std::stoll(trimmed);
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), val, true);
    }

    bool isFloat = !trimmed.empty();
    bool hasDot = false;
    for (size_t i = 0; i < trimmed.size(); i++) {
        char c = trimmed[i];
        if (c == '-' && i == 0) continue;
        if (c == '.' && !hasDot) { hasDot = true; continue; }
        if (!std::isdigit(c)) { isFloat = false; break; }
    }

    if (isFloat && hasDot) {
        double val = std::stod(trimmed);
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), val);
    }

    if (trimmed.size() >= 2 && (trimmed.front() == '"' || trimmed.front() == '\'')) {
        std::string strVal = trimmed.substr(1, trimmed.size() - 2);
        return builder_->CreateGlobalStringPtr(strVal);
    }

    auto localIt = localVars.find(trimmed);
    if (localIt != localVars.end()) {
        return localIt->second;
    }

    auto namedIt = namedValues_.find(trimmed);
    if (namedIt != namedValues_.end()) {
        return builder_->CreateLoad(namedIt->second->getAllocatedType(), namedIt->second, trimmed);
    }

    return nullptr;
}

llvm::AllocaInst* SectionCodeGenerator::createEntryAlloca(
    llvm::Function* function,
    const std::string& name,
    llvm::Type* type
) {
    llvm::IRBuilder<> tmpBuilder(
        &function->getEntryBlock(),
        function->getEntryBlock().begin()
    );
    return tmpBuilder.CreateAlloca(type, nullptr, name);
}

} // namespace tbx
