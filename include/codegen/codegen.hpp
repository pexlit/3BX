#pragma once

#include "ast/ast.hpp"
#include "pattern/pattern_registry.hpp"
#include "pattern/pattern_matcher.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace tbx {

class CodeGenerator : public ASTVisitor {
public:
    CodeGenerator(const std::string& module_name);

    // Set the pattern registry for resolving NaturalExpr at codegen time
    void setPatternRegistry(PatternRegistry* registry);

    // Generate LLVM IR for program
    bool generate(Program& program);

    // Get the generated module
    llvm::Module* getModule() { return module_.get(); }

    // Take ownership of the module (for JIT execution)
    std::unique_ptr<llvm::Module> takeModule() { return std::move(module_); }

    // Take ownership of the context (for JIT execution)
    std::unique_ptr<llvm::LLVMContext> takeContext() { return std::move(context_); }

    // Get libraries used by the program (for linking)
    const std::set<std::string>& getUsedLibraries() const { return usedLibraries_; }

    // Output LLVM IR to file
    bool writeIr(const std::string& filename);

    // Compile to object file
    bool compile(const std::string& filename);

    // Visitor methods
    void visit(IntegerLiteral& node) override;
    void visit(FloatLiteral& node) override;
    void visit(StringLiteral& node) override;
    void visit(Identifier& node) override;
    void visit(BooleanLiteral& node) override;
    void visit(UnaryExpr& node) override;
    void visit(BinaryExpr& node) override;
    void visit(NaturalExpr& node) override;
    void visit(LazyExpr& node) override;
    void visit(BlockExpr& node) override;
    void visit(ExpressionStmt& node) override;
    void visit(SetStatement& node) override;
    void visit(IfStatement& node) override;
    void visit(WhileStatement& node) override;
    void visit(FunctionDecl& node) override;
    void visit(IntrinsicCall& node) override;
    void visit(PatternDef& node) override;
    void visit(PatternCall& node) override;
    void visit(ImportStmt& node) override;
    void visit(UseStmt& node) override;
    void visit(ImportFunctionDecl& node) override;
    void visit(Program& node) override;

private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;

    // Symbol table for variables
    std::unordered_map<std::string, llvm::AllocaInst*> named_values_;

    // Current value being built
    llvm::Value* current_value_ = nullptr;

    // Registry for imported external functions (FFI)
    struct ImportedFunction {
        std::string name;                    // Function name
        std::vector<std::string> params;     // Parameter names
        std::string header;                  // Header file
        llvm::Function* llvmFunc = nullptr;  // LLVM function declaration
    };
    std::unordered_map<std::string, ImportedFunction> importedFunctions_;

    // Set of libraries used (for linking)
    std::set<std::string> usedLibraries_;

    // Tail-call optimization for recursive patterns
    // Tracks the current pattern execution context for TCO
    struct PatternContext {
        PatternDef* pattern = nullptr;           // Current pattern being executed
        llvm::BasicBlock* loopHeader = nullptr;  // Jump target for tail recursion
        std::unordered_map<std::string, llvm::AllocaInst*>* bindings = nullptr;
        bool inTailPosition = false;             // Whether we're in a tail position
    };
    std::vector<PatternContext> patternStack_;

    // Check if a statement is a tail call to the current pattern
    bool isTailRecursion(Statement* stmt);

    // Deferred bindings for BlockExpr and LazyExpr (not evaluated until used by intrinsics)
    std::unordered_map<std::string, Expression*> deferredBindings_;

    // Helper methods
    llvm::AllocaInst* createEntryBlockAlloca(
        llvm::Function* function,
        const std::string& name,
        llvm::Type* type
    );

    // Create LLVM function declaration for an imported function
    llvm::Function* declareExternalFunction(const std::string& name, size_t paramCount);

    // Resolve intrinsic argument - checks if it's an Identifier that maps to a deferred binding
    Expression* resolveDeferredBinding(Expression* arg);

    // Pattern registry for resolving NaturalExpr at codegen time
    PatternRegistry* patternRegistry_ = nullptr;
    std::unique_ptr<PatternMatcher> patternMatcher_;
};

} // namespace tbx
