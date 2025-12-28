#include "codegen/codegen.hpp"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Verifier.h>

namespace tbx {

CodeGenerator::CodeGenerator(const std::string& module_name) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(module_name, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
}

void CodeGenerator::setPatternRegistry(PatternRegistry* registry) {
    patternRegistry_ = registry;
    if (registry) {
        patternMatcher_ = std::make_unique<PatternMatcher>(*registry);
    }
}

bool CodeGenerator::generate(Program& program) {
    program.accept(*this);
    return !llvm::verifyModule(*module_, &llvm::errs());
}

bool CodeGenerator::writeIr(const std::string& filename) {
    std::error_code ec;
    llvm::raw_fd_ostream out(filename, ec, llvm::sys::fs::OF_None);
    if (ec) {
        return false;
    }
    module_->print(out, nullptr);
    return true;
}

bool CodeGenerator::compile(const std::string& filename) {
    // TODO: Implement native code generation
    (void)filename;
    return false;
}

llvm::AllocaInst* CodeGenerator::createEntryBlockAlloca(
    llvm::Function* function,
    const std::string& name,
    llvm::Type* type
) {
    llvm::IRBuilder<> tmp_builder(
        &function->getEntryBlock(),
        function->getEntryBlock().begin()
    );
    return tmp_builder.CreateAlloca(type, nullptr, name);
}

void CodeGenerator::visit(IntegerLiteral& node) {
    current_value_ = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(*context_),
        node.value,
        true
    );
}

void CodeGenerator::visit(FloatLiteral& node) {
    current_value_ = llvm::ConstantFP::get(
        llvm::Type::getDoubleTy(*context_),
        node.value
    );
}

void CodeGenerator::visit(StringLiteral& node) {
    current_value_ = builder_->CreateGlobalStringPtr(node.value);
}

void CodeGenerator::visit(Identifier& node) {
    auto it = named_values_.find(node.name);
    if (it != named_values_.end()) {
        current_value_ = builder_->CreateLoad(
            it->second->getAllocatedType(),
            it->second,
            node.name
        );
    } else {
        current_value_ = nullptr;
    }
}

void CodeGenerator::visit(BooleanLiteral& node) {
    current_value_ = llvm::ConstantInt::get(
        llvm::Type::getInt1Ty(*context_),
        node.value ? 1 : 0
    );
}

void CodeGenerator::visit(UnaryExpr& node) {
    // UnaryExpr should not be created by the parser anymore.
    // All operators should be matched as patterns and create PatternCall nodes.
    // This is kept as a fallback but should not be reached in normal operation.

    // For backwards compatibility during transition, generate a placeholder
    current_value_ = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
}

void CodeGenerator::visit(BinaryExpr& node) {
    // BinaryExpr should not be created by the parser anymore.
    // All operators should be matched as patterns and create PatternCall nodes.
    // This is kept as a fallback but should not be reached in normal operation.
    // If we get here, it means the parser created a BinaryExpr directly,
    // which violates the LANGUAGE.md design principle.

    // For backwards compatibility during transition, generate a placeholder
    current_value_ = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
}

void CodeGenerator::visit(NaturalExpr& node) {
    // Natural language expressions need pattern matching to resolve
    // Try to match the tokens against registered patterns
    if (patternMatcher_ && !node.tokens.empty()) {
        auto result = patternMatcher_->matchStatement(node.tokens, 0);
        if (result && result->pattern && result->pattern->definition) {
            // Found a matching pattern - create a PatternCall and generate code for it
            PatternCall call;
            call.pattern = result->pattern->definition;
            call.bindings = std::move(result->bindings);
            call.location = node.location;

            // Generate code for the pattern call
            visit(call);
            return;
        }
    }

    // No pattern matched - generate a placeholder value (0)
    current_value_ = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
}

void CodeGenerator::visit(LazyExpr& node) {
    // Lazy expressions store the inner expression for deferred evaluation
    // When codegen encounters a lazy expr directly, we just evaluate it
    // The lazy semantics are handled by the pattern caller
    if (node.inner) {
        node.inner->accept(*this);
    } else {
        current_value_ = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
    }
}

void CodeGenerator::visit(BlockExpr& node) {
    // Block expressions contain a list of statements
    // Generate code for each statement in the block
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    // Block doesn't produce a value
    current_value_ = nullptr;
}

void CodeGenerator::visit(ExpressionStmt& node) {
    node.expression->accept(*this);
}

void CodeGenerator::visit(SetStatement& node) {
    node.value->accept(*this);
    llvm::Value* value = current_value_;
    if (!value) return;

    auto it = named_values_.find(node.variable);
    if (it != named_values_.end()) {
        builder_->CreateStore(value, it->second);
    } else {
        // Create new variable
        llvm::Function* function = builder_->GetInsertBlock()->getParent();
        llvm::AllocaInst* alloca = createEntryBlockAlloca(
            function,
            node.variable,
            value->getType()
        );
        builder_->CreateStore(value, alloca);
        named_values_[node.variable] = alloca;
    }
}

void CodeGenerator::visit(IfStatement& node) {
    node.condition->accept(*this);
    llvm::Value* cond = current_value_;
    if (!cond) return;

    llvm::Function* function = builder_->GetInsertBlock()->getParent();

    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*context_, "then", function);
    llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*context_, "else");
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "ifcont");

    builder_->CreateCondBr(cond, then_bb, else_bb);

    // Then block
    builder_->SetInsertPoint(then_bb);
    for (auto& stmt : node.then_branch) {
        stmt->accept(*this);
    }
    builder_->CreateBr(merge_bb);

    // Else block
    function->insert(function->end(), else_bb);
    builder_->SetInsertPoint(else_bb);
    for (auto& stmt : node.else_branch) {
        stmt->accept(*this);
    }
    builder_->CreateBr(merge_bb);

    // Merge block
    function->insert(function->end(), merge_bb);
    builder_->SetInsertPoint(merge_bb);
}

void CodeGenerator::visit(WhileStatement& node) {
    llvm::Function* function = builder_->GetInsertBlock()->getParent();

    llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*context_, "while.cond", function);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "while.body");
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*context_, "while.end");

    // Branch to condition block
    builder_->CreateBr(cond_bb);

    // Condition block
    builder_->SetInsertPoint(cond_bb);
    node.condition->accept(*this);
    llvm::Value* cond = current_value_;
    if (!cond) {
        cond = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0);
    }
    // Convert to boolean if needed (compare with 0)
    if (!cond->getType()->isIntegerTy(1)) {
        if (cond->getType()->isIntegerTy()) {
            cond = builder_->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0), "tobool");
        } else if (cond->getType()->isDoubleTy()) {
            cond = builder_->CreateFCmpONE(cond, llvm::ConstantFP::get(cond->getType(), 0.0), "tobool");
        }
    }
    builder_->CreateCondBr(cond, body_bb, end_bb);

    // Body block
    function->insert(function->end(), body_bb);
    builder_->SetInsertPoint(body_bb);
    for (auto& stmt : node.body) {
        stmt->accept(*this);
    }
    builder_->CreateBr(cond_bb);  // Loop back to condition

    // End block
    function->insert(function->end(), end_bb);
    builder_->SetInsertPoint(end_bb);
}

void CodeGenerator::visit(FunctionDecl& node) {
    // Create function type
    std::vector<llvm::Type*> param_types(node.params.size(), llvm::Type::getInt64Ty(*context_));
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(*context_),
        param_types,
        false
    );

    llvm::Function* function = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        node.name,
        module_.get()
    );

    // Set parameter names
    size_t idx = 0;
    for (auto& arg : function->args()) {
        arg.setName(node.params[idx++]);
    }

    // Create entry block
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", function);
    builder_->SetInsertPoint(entry);

    // Add parameters to symbol table
    named_values_.clear();
    for (auto& arg : function->args()) {
        llvm::AllocaInst* alloca = createEntryBlockAlloca(
            function,
            std::string(arg.getName()),
            arg.getType()
        );
        builder_->CreateStore(&arg, alloca);
        named_values_[std::string(arg.getName())] = alloca;
    }

    // Generate body
    for (auto& stmt : node.body) {
        stmt->accept(*this);
    }

    // Default return
    builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0));
}

// Helper to resolve an intrinsic argument - checks if it's an Identifier that maps to a deferred binding
Expression* CodeGenerator::resolveDeferredBinding(Expression* arg) {
    auto* id = dynamic_cast<Identifier*>(arg);
    if (id) {
        auto it = deferredBindings_.find(id->name);
        if (it != deferredBindings_.end()) {
            return it->second;
        }
    }
    return arg;
}

void CodeGenerator::visit(IntrinsicCall& node) {
    // Map intrinsic names to LLVM operations
    if (node.name == "store") {
        // @intrinsic("store", var_name, value)
        if (node.args.size() >= 2) {
            // Get variable name
            auto* id = dynamic_cast<Identifier*>(node.args[0].get());
            if (id) {
                node.args[1]->accept(*this);
                llvm::Value* value = current_value_;
                if (value) {
                    auto it = named_values_.find(id->name);
                    if (it != named_values_.end()) {
                        builder_->CreateStore(value, it->second);
                    } else {
                        llvm::Function* function = builder_->GetInsertBlock()->getParent();
                        llvm::AllocaInst* alloca = createEntryBlockAlloca(
                            function, id->name, value->getType());
                        builder_->CreateStore(value, alloca);
                        named_values_[id->name] = alloca;
                    }
                }
            }
        }
        current_value_ = nullptr;
    }
    else if (node.name == "load") {
        if (node.args.size() >= 1) {
            auto* id = dynamic_cast<Identifier*>(node.args[0].get());
            if (id) {
                auto it = named_values_.find(id->name);
                if (it != named_values_.end()) {
                    current_value_ = builder_->CreateLoad(
                        it->second->getAllocatedType(), it->second, id->name);
                } else {
                    current_value_ = nullptr;
                }
            }
        }
    }
    else if (node.name == "add") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFAdd(left, right, "addtmp");
                } else {
                    current_value_ = builder_->CreateAdd(left, right, "addtmp");
                }
            }
        }
    }
    else if (node.name == "sub") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFSub(left, right, "subtmp");
                } else {
                    current_value_ = builder_->CreateSub(left, right, "subtmp");
                }
            }
        }
    }
    else if (node.name == "mul") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFMul(left, right, "multmp");
                } else {
                    current_value_ = builder_->CreateMul(left, right, "multmp");
                }
            }
        }
    }
    else if (node.name == "div") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFDiv(left, right, "divtmp");
                } else {
                    current_value_ = builder_->CreateSDiv(left, right, "divtmp");
                }
            }
        }
    }
    else if (node.name == "cmp_lt") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFCmpOLT(left, right, "cmptmp");
                } else {
                    current_value_ = builder_->CreateICmpSLT(left, right, "cmptmp");
                }
            }
        }
    }
    else if (node.name == "cmp_gt") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFCmpOGT(left, right, "cmptmp");
                } else {
                    current_value_ = builder_->CreateICmpSGT(left, right, "cmptmp");
                }
            }
        }
    }
    else if (node.name == "cmp_eq") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFCmpOEQ(left, right, "eqtmp");
                } else {
                    current_value_ = builder_->CreateICmpEQ(left, right, "eqtmp");
                }
            }
        }
    }
    else if (node.name == "cmp_neq") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFCmpONE(left, right, "netmp");
                } else {
                    current_value_ = builder_->CreateICmpNE(left, right, "netmp");
                }
            }
        }
    }
    else if (node.name == "cmp_lte") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFCmpOLE(left, right, "cmptmp");
                } else {
                    current_value_ = builder_->CreateICmpSLE(left, right, "cmptmp");
                }
            }
        }
    }
    else if (node.name == "cmp_gte") {
        if (node.args.size() >= 2) {
            node.args[0]->accept(*this);
            llvm::Value* left = current_value_;
            node.args[1]->accept(*this);
            llvm::Value* right = current_value_;
            if (left && right) {
                bool left_is_float = left->getType()->isFloatingPointTy();
                bool right_is_float = right->getType()->isFloatingPointTy();
                bool use_float = left_is_float || right_is_float;
                if (use_float) {
                    llvm::Type* double_type = llvm::Type::getDoubleTy(*context_);
                    if (!left_is_float) left = builder_->CreateSIToFP(left, double_type, "conv");
                    if (!right_is_float) right = builder_->CreateSIToFP(right, double_type, "conv");
                    current_value_ = builder_->CreateFCmpOGE(left, right, "cmptmp");
                } else {
                    current_value_ = builder_->CreateICmpSGE(left, right, "cmptmp");
                }
            }
        }
    }
    else if (node.name == "print") {
        // Get or create printf declaration
        llvm::FunctionType* printf_type = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(*context_),
            {llvm::PointerType::get(llvm::Type::getInt8Ty(*context_), 0)},
            true
        );
        llvm::FunctionCallee printf_func = module_->getOrInsertFunction("printf", printf_type);

        if (node.args.size() >= 1) {
            node.args[0]->accept(*this);
            llvm::Value* val = current_value_;
            if (val) {
                llvm::Value* format_str;
                // Choose format based on value type
                if (val->getType()->isPointerTy()) {
                    format_str = builder_->CreateGlobalStringPtr("%s\n");
                } else if (val->getType()->isDoubleTy() || val->getType()->isFloatTy()) {
                    format_str = builder_->CreateGlobalStringPtr("%f\n");
                } else {
                    format_str = builder_->CreateGlobalStringPtr("%lld\n");
                }
                builder_->CreateCall(printf_func, {format_str, val});
            }
        }
        current_value_ = nullptr;
    }
    else if (node.name == "execute") {
        // @intrinsic("execute", block) - Execute a captured code block
        // The block is a BlockExpr containing statements to execute
        if (node.args.size() >= 1) {
            // Resolve the argument (may be an Identifier referring to a deferred binding)
            Expression* resolved = resolveDeferredBinding(node.args[0].get());
            auto* blockExpr = dynamic_cast<BlockExpr*>(resolved);
            if (blockExpr) {
                // Generate code for each statement in the block
                for (auto& stmt : blockExpr->statements) {
                    stmt->accept(*this);
                }
            } else {
                // If not a BlockExpr, just evaluate the expression
                resolved->accept(*this);
            }
        }
        current_value_ = nullptr;
    }
    else if (node.name == "evaluate") {
        // @intrinsic("evaluate", expr) - Evaluate a lazy expression
        // Unwraps a LazyExpr and evaluates its inner expression
        if (node.args.size() >= 1) {
            // Resolve the argument (may be an Identifier referring to a deferred binding)
            Expression* resolved = resolveDeferredBinding(node.args[0].get());
            auto* lazyExpr = dynamic_cast<LazyExpr*>(resolved);
            if (lazyExpr && lazyExpr->inner) {
                // Evaluate the inner expression
                lazyExpr->inner->accept(*this);
            } else {
                // Not a LazyExpr, just evaluate directly
                resolved->accept(*this);
            }
        } else {
            current_value_ = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
        }
    }
    else if (node.name == "execute_if") {
        // @intrinsic("execute_if", condition, block) - Execute block only if condition is true
        // Takes a lazy condition and a block, evaluates condition, executes block only if true
        if (node.args.size() >= 2) {
            // Resolve arguments (may be Identifiers referring to deferred bindings)
            Expression* condArg = resolveDeferredBinding(node.args[0].get());
            Expression* blockArg = resolveDeferredBinding(node.args[1].get());

            // Evaluate the condition (may be lazy)
            auto* lazyExpr = dynamic_cast<LazyExpr*>(condArg);
            if (lazyExpr && lazyExpr->inner) {
                lazyExpr->inner->accept(*this);
            } else {
                condArg->accept(*this);
            }
            llvm::Value* cond = current_value_;
            if (!cond) {
                current_value_ = nullptr;
                return;
            }

            // Convert to boolean if needed
            if (!cond->getType()->isIntegerTy(1)) {
                if (cond->getType()->isIntegerTy()) {
                    cond = builder_->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0), "tobool");
                } else if (cond->getType()->isDoubleTy()) {
                    cond = builder_->CreateFCmpONE(cond, llvm::ConstantFP::get(cond->getType(), 0.0), "tobool");
                }
            }

            llvm::Function* function = builder_->GetInsertBlock()->getParent();
            llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context_, "execute_if.then", function);
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*context_, "execute_if.end");

            builder_->CreateCondBr(cond, thenBB, endBB);

            // Then block - execute the block
            builder_->SetInsertPoint(thenBB);
            auto* blockExpr = dynamic_cast<BlockExpr*>(blockArg);
            if (blockExpr) {
                for (auto& stmt : blockExpr->statements) {
                    stmt->accept(*this);
                }
            } else {
                blockArg->accept(*this);
            }
            builder_->CreateBr(endBB);

            // End block
            function->insert(function->end(), endBB);
            builder_->SetInsertPoint(endBB);
        }
        current_value_ = nullptr;
    }
    else if (node.name == "loop_while") {
        // @intrinsic("loop_while", condition, block) - Loop while condition is true
        // Generates proper LLVM loop: evaluate condition, if true execute block and repeat
        if (node.args.size() >= 2) {
            // Resolve arguments (may be Identifiers referring to deferred bindings)
            Expression* condArg = resolveDeferredBinding(node.args[0].get());
            Expression* blockArg = resolveDeferredBinding(node.args[1].get());

            llvm::Function* function = builder_->GetInsertBlock()->getParent();

            llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*context_, "loop_while.cond", function);
            llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*context_, "loop_while.body");
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*context_, "loop_while.end");

            // Branch to condition block
            builder_->CreateBr(condBB);

            // Condition block - evaluate the condition
            builder_->SetInsertPoint(condBB);
            auto* lazyExpr = dynamic_cast<LazyExpr*>(condArg);
            if (lazyExpr && lazyExpr->inner) {
                lazyExpr->inner->accept(*this);
            } else {
                condArg->accept(*this);
            }
            llvm::Value* cond = current_value_;
            if (!cond) {
                cond = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0);
            }

            // Convert to boolean if needed
            if (!cond->getType()->isIntegerTy(1)) {
                if (cond->getType()->isIntegerTy()) {
                    cond = builder_->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(), 0), "tobool");
                } else if (cond->getType()->isDoubleTy()) {
                    cond = builder_->CreateFCmpONE(cond, llvm::ConstantFP::get(cond->getType(), 0.0), "tobool");
                }
            }
            builder_->CreateCondBr(cond, bodyBB, endBB);

            // Body block - execute the block and loop back
            function->insert(function->end(), bodyBB);
            builder_->SetInsertPoint(bodyBB);
            auto* blockExpr = dynamic_cast<BlockExpr*>(blockArg);
            if (blockExpr) {
                for (auto& stmt : blockExpr->statements) {
                    stmt->accept(*this);
                }
            } else {
                blockArg->accept(*this);
            }
            builder_->CreateBr(condBB);  // Loop back to condition

            // End block
            function->insert(function->end(), endBB);
            builder_->SetInsertPoint(endBB);
        }
        current_value_ = nullptr;
    }
    else if (node.name == "call") {
        // @intrinsic("call", "LIBRARY", "functionName", arg1, arg2, ...)
        // First argument is the library name (string)
        // Second argument is the function name (string)
        // Remaining arguments are function arguments
        if (node.args.size() < 2) {
            current_value_ = nullptr;
            return;
        }

        // Get library name from first argument
        auto* libNameLit = dynamic_cast<StringLiteral*>(node.args[0].get());
        if (!libNameLit) {
            current_value_ = nullptr;
            return;
        }
        std::string libName = libNameLit->value;

        // Get function name from second argument
        auto* funcNameLit = dynamic_cast<StringLiteral*>(node.args[1].get());
        if (!funcNameLit) {
            current_value_ = nullptr;
            return;
        }
        std::string funcName = funcNameLit->value;

        // Track the library for linking
        usedLibraries_.insert(libName);

        // Look up the imported function
        auto it = importedFunctions_.find(funcName);
        if (it == importedFunctions_.end()) {
            // Function not imported - declare it with the argument count
            size_t argCount = node.args.size() - 2;
            declareExternalFunction(funcName, argCount);
            it = importedFunctions_.find(funcName);
            if (it == importedFunctions_.end()) {
                current_value_ = nullptr;
                return;
            }
        }

        llvm::Function* func = it->second.llvmFunc;
        if (!func) {
            current_value_ = nullptr;
            return;
        }

        // Evaluate and collect arguments (skip library and function name)
        std::vector<llvm::Value*> args;
        for (size_t i = 2; i < node.args.size(); i++) {
            node.args[i]->accept(*this);
            if (!current_value_) {
                current_value_ = nullptr;
                return;
            }

            llvm::Value* argVal = current_value_;

            // Convert argument to expected type (double for FFI)
            llvm::Type* expectedType = llvm::Type::getDoubleTy(*context_);
            if (argVal->getType() != expectedType) {
                if (argVal->getType()->isIntegerTy()) {
                    argVal = builder_->CreateSIToFP(argVal, expectedType, "conv");
                } else if (argVal->getType()->isFloatTy()) {
                    argVal = builder_->CreateFPExt(argVal, expectedType, "conv");
                }
            }

            args.push_back(argVal);
        }

        // Call the function
        current_value_ = builder_->CreateCall(func, args, "calltmp");
    }
    else {
        // Unknown intrinsic
        current_value_ = nullptr;
    }
}

void CodeGenerator::visit(PatternDef& node) {
    // Pattern definitions are not directly compiled
    // They are used by the pattern matcher during parsing
    (void)node;
}

bool CodeGenerator::isTailRecursion(Statement* stmt) {
    // Check if a statement is a recursive call to the current pattern
    auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt);
    if (!exprStmt || !exprStmt->expression) return false;

    auto* patternCall = dynamic_cast<PatternCall*>(exprStmt->expression.get());
    if (!patternCall || !patternCall->pattern) return false;

    // Check if this is calling the same pattern we're currently executing
    if (patternStack_.empty()) return false;
    return patternCall->pattern == patternStack_.back().pattern;
}

void CodeGenerator::visit(PatternCall& node) {
    // Execute the pattern's when_triggered body with bindings
    // With tail-call optimization for recursive patterns

    if (!node.pattern) {
        current_value_ = nullptr;
        return;
    }

    // Check if this is a tail-recursive call
    if (!patternStack_.empty()) {
        auto& ctx = patternStack_.back();
        if (ctx.inTailPosition && ctx.pattern == node.pattern && ctx.loopHeader) {
            // This is a tail-recursive call - update bindings and jump back
            for (auto& [name, expr] : node.bindings) {
                expr->accept(*this);
                if (current_value_ && ctx.bindings) {
                    auto it = ctx.bindings->find(name);
                    if (it != ctx.bindings->end()) {
                        builder_->CreateStore(current_value_, it->second);
                    }
                }
            }
            // Jump back to loop header
            builder_->CreateBr(ctx.loopHeader);
            current_value_ = nullptr;
            return;
        }
    }

    // Save current named_values
    auto saved_values = named_values_;

    // Store BlockExpr and LazyExpr bindings for later access by intrinsics
    // These should not be evaluated/executed during binding setup
    std::unordered_map<std::string, Expression*> deferredBindings;

    // Create allocas for bindings and evaluate initial values
    std::unordered_map<std::string, llvm::AllocaInst*> patternBindings;
    llvm::Function* function = builder_->GetInsertBlock()->getParent();

    for (auto& [name, expr] : node.bindings) {
        // BlockExpr and LazyExpr should NOT be evaluated here - store for later
        if (dynamic_cast<BlockExpr*>(expr.get()) || dynamic_cast<LazyExpr*>(expr.get())) {
            deferredBindings[name] = expr.get();
            continue;
        }

        expr->accept(*this);
        if (current_value_) {
            llvm::AllocaInst* alloca = createEntryBlockAlloca(
                function, name, current_value_->getType());
            builder_->CreateStore(current_value_, alloca);
            named_values_[name] = alloca;
            patternBindings[name] = alloca;
        }
    }

    // Store deferred bindings in a member variable so intrinsics can access them
    deferredBindings_ = std::move(deferredBindings);

    // Create basic blocks for the pattern body loop (for tail-call optimization)
    llvm::BasicBlock* patternBodyBB = llvm::BasicBlock::Create(*context_, "pattern.body", function);
    llvm::BasicBlock* patternEndBB = llvm::BasicBlock::Create(*context_, "pattern.end");

    // Jump to pattern body
    builder_->CreateBr(patternBodyBB);
    builder_->SetInsertPoint(patternBodyBB);

    // Set up pattern context for tail-call optimization
    PatternContext ctx;
    ctx.pattern = node.pattern;
    ctx.loopHeader = patternBodyBB;
    ctx.bindings = &patternBindings;
    ctx.inTailPosition = false;
    patternStack_.push_back(ctx);

    // Generate code for when_triggered body with tail-call detection
    auto& stmts = node.pattern->when_triggered;
    for (size_t i = 0; i < stmts.size(); ++i) {
        // Mark the last statement as being in tail position
        if (i == stmts.size() - 1) {
            patternStack_.back().inTailPosition = true;
        }

        stmts[i]->accept(*this);

        // If the last statement was a tail call, it already added a branch
        // and we should not add a fallthrough
        if (i == stmts.size() - 1 && isTailRecursion(stmts[i].get())) {
            // The recursive call already created the branch - don't add fallthrough
        }
    }

    patternStack_.pop_back();

    // Only add fallthrough if the block doesn't already have a terminator
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateBr(patternEndBB);
    }

    // Continue after pattern
    function->insert(function->end(), patternEndBB);
    builder_->SetInsertPoint(patternEndBB);

    // For expression patterns, return the 'result' variable value
    auto result_it = named_values_.find("result");
    if (result_it != named_values_.end()) {
        current_value_ = builder_->CreateLoad(
            result_it->second->getAllocatedType(),
            result_it->second,
            "result_val");
    } else {
        current_value_ = nullptr;
    }

    // Restore named_values
    named_values_ = saved_values;
}

void CodeGenerator::visit(ImportStmt& node) {
    // Imports are handled during parsing
    (void)node;
}

void CodeGenerator::visit(UseStmt& node) {
    // Use statements are handled during parsing
    (void)node;
}

void CodeGenerator::visit(ImportFunctionDecl& node) {
    // Register the imported function for later use by the "call" intrinsic
    ImportedFunction imported;
    imported.name = node.name;
    imported.params = node.params;
    imported.header = node.header;

    // Create LLVM function declaration
    imported.llvmFunc = declareExternalFunction(node.name, node.params.size());

    importedFunctions_[node.name] = imported;
}

llvm::Function* CodeGenerator::declareExternalFunction(const std::string& name, size_t paramCount) {
    // Check if already declared in module
    llvm::Function* func = module_->getFunction(name);
    if (!func) {
        // Create function type with double parameters (common for graphics APIs)
        // For flexibility, we use double for all parameters since it can represent
        // both integers and floating point values
        std::vector<llvm::Type*> paramTypes(paramCount, llvm::Type::getDoubleTy(*context_));

        // Return type is double by default (can be extended later for type info)
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            llvm::Type::getDoubleTy(*context_),
            paramTypes,
            false
        );

        // Create external function declaration
        func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            name,
            module_.get()
        );
    }

    // Register in importedFunctions_ if not already there
    if (importedFunctions_.find(name) == importedFunctions_.end()) {
        ImportedFunction imported;
        imported.name = name;
        imported.llvmFunc = func;
        importedFunctions_[name] = imported;
    }

    return func;
}

void CodeGenerator::visit(Program& node) {
    // Create main function for top-level statements
    llvm::FunctionType* main_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context_),
        {},
        false
    );

    llvm::Function* main_func = llvm::Function::Create(
        main_type,
        llvm::Function::ExternalLinkage,
        "main",
        module_.get()
    );

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", main_func);
    builder_->SetInsertPoint(entry);

    // Generate code for all statements
    for (auto& stmt : node.statements) {
        // Skip pattern definitions (they're just metadata)
        if (dynamic_cast<PatternDef*>(stmt.get())) {
            continue;
        }
        stmt->accept(*this);
    }

    // Return 0 from main
    builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
}

} // namespace tbx
