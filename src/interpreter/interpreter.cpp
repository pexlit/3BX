#include "interpreter/interpreter.hpp"
#include "pattern/pattern_registry.hpp"
#include "pattern/pattern_matcher.hpp"

#include <iostream>
#include <sstream>
#include <cmath>

namespace tbx {

// Value implementation
std::string Value::toString() const {
    if (isNull()) return "null";
    if (isInt()) return std::to_string(asInt());
    if (isFloat()) {
        std::ostringstream oss;
        oss << asFloat();
        return oss.str();
    }
    if (isString()) return asString();
    if (isBool()) return asBool() ? "true" : "false";
    return "unknown";
}

std::string Value::typeName() const {
    if (isNull()) return "null";
    if (isInt()) return "int";
    if (isFloat()) return "float";
    if (isString()) return "string";
    if (isBool()) return "bool";
    return "unknown";
}

bool Value::isTruthy() const {
    if (isNull()) return false;
    if (isBool()) return asBool();
    if (isInt()) return asInt() != 0;
    if (isFloat()) return asFloat() != 0.0;
    if (isString()) return !asString().empty();
    return true;
}

// Interpreter implementation
Interpreter::Interpreter() = default;
Interpreter::~Interpreter() = default;

void Interpreter::load(Program* program) {
    program_ = program;
    state_ = ExecutionState::Idle;
    errorMessage_.clear();
    globals_.clear();
    functions_.clear();
    callStack_.clear();
    currentLocals_ = nullptr;
    deferredBindings_.clear();
}

void Interpreter::setPatternRegistry(PatternRegistry* registry) {
    patternRegistry_ = registry;
    if (registry) {
        patternMatcher_ = std::make_unique<PatternMatcher>(*registry);
    } else {
        patternMatcher_.reset();
    }
}

void Interpreter::run() {
    if (!program_) {
        setError("No program loaded");
        return;
    }

    state_ = ExecutionState::Running;

    // Push main frame
    CallFrame mainFrame;
    mainFrame.name = "<main>";
    mainFrame.location.line = 1;
    mainFrame.location.column = 1;
    mainFrame.location.filename = program_->location.filename;
    mainFrame.locals = &globals_;
    callStack_.push_back(mainFrame);
    currentLocals_ = &globals_;

    try {
        for (auto& stmt : program_->statements) {
            if (state_ == ExecutionState::Error) break;

            // First pass: collect function declarations
            if (auto* funcDecl = dynamic_cast<FunctionDecl*>(stmt.get())) {
                functions_[funcDecl->name] = funcDecl;
                continue;
            }

            executeStatement(*stmt);

            // Check if we should stop
            if (state_ == ExecutionState::Finished || state_ == ExecutionState::Error) {
                break;
            }
        }

        if (state_ == ExecutionState::Running) {
            state_ = ExecutionState::Finished;
        }
    } catch (const std::exception& e) {
        setError(e.what());
    }

    callStack_.pop_back();
    stateChanged_.notify_all();
}

void Interpreter::pause() {
    if (state_ == ExecutionState::Running) {
        state_ = ExecutionState::Paused;
        stateChanged_.notify_all();
    }
}

void Interpreter::resume() {
    if (state_ == ExecutionState::Paused) {
        stepMode_ = StepMode::None;
        state_ = ExecutionState::Running;
        stateChanged_.notify_all();
    }
}

void Interpreter::step(StepMode mode) {
    if (state_ == ExecutionState::Paused) {
        stepMode_ = mode;
        stepDepth_ = (int)callStack_.size();
        state_ = ExecutionState::Running;
        stateChanged_.notify_all();
    }
}

void Interpreter::stop() {
    state_ = ExecutionState::Finished;
    stateChanged_.notify_all();
}

void Interpreter::waitForStateChange(ExecutionState expected) {
    std::unique_lock<std::mutex> lock(stateMutex_);
    stateChanged_.wait(lock, [this, expected]() {
        return state_ != expected;
    });
}

std::vector<std::pair<std::string, Value>> Interpreter::getLocals() const {
    std::vector<std::pair<std::string, Value>> result;
    if (currentLocals_) {
        for (const auto& pair : *currentLocals_) {
            result.push_back(pair);
        }
    }
    return result;
}

std::vector<std::pair<std::string, Value>> Interpreter::getGlobals() const {
    std::vector<std::pair<std::string, Value>> result;
    for (const auto& pair : globals_) {
        result.push_back(pair);
    }
    return result;
}

Value Interpreter::getVariable(const std::string& name) const {
    // Check locals first
    if (currentLocals_ && currentLocals_->count(name)) {
        return currentLocals_->at(name);
    }
    // Then globals
    if (globals_.count(name)) {
        return globals_.at(name);
    }
    return Value();
}

Value Interpreter::evaluate(const std::string& expr) {
    // Simple expression evaluation for debugging
    // For now, just try to look up as a variable
    return getVariable(expr);
}

void Interpreter::executeStatement(Statement& stmt) {
    updateLocation(stmt);
    checkBreakpoint(stmt.location);

    // Wait if paused
    while (state_ == ExecutionState::Paused) {
        std::unique_lock<std::mutex> lock(stateMutex_);
        stateChanged_.wait(lock, [this]() {
            return state_ != ExecutionState::Paused;
        });
    }

    if (state_ != ExecutionState::Running) {
        return;
    }

    stmt.accept(*this);
}

Value Interpreter::evaluateExpression(Expression& expr) {
    expr.accept(*this);
    return currentValue_;
}

void Interpreter::checkBreakpoint(const SourceLocation& location) {
    // Check if we hit a breakpoint or step
    bool shouldPause = false;

    if (breakpointCallback_ && breakpointCallback_(location)) {
        shouldPause = true;
    }

    // Check step mode
    if (stepMode_ != StepMode::None) {
        int currentDepth = (int)callStack_.size();

        switch (stepMode_) {
            case StepMode::In:
                shouldPause = true;
                break;
            case StepMode::Over:
                shouldPause = (currentDepth <= stepDepth_);
                break;
            case StepMode::Out:
                shouldPause = (currentDepth < stepDepth_);
                break;
            default:
                break;
        }
    }

    if (shouldPause) {
        stepMode_ = StepMode::None;
        state_ = ExecutionState::Paused;
        stateChanged_.notify_all();
    }
}

void Interpreter::updateLocation(const ASTNode& node) {
    currentLocation_ = node.location;
    if (!callStack_.empty()) {
        callStack_.back().location = currentLocation_;
    }
}

void Interpreter::output(const std::string& text) {
    if (outputCallback_) {
        outputCallback_(text);
    } else {
        std::cout << text;
    }
}

void Interpreter::setError(const std::string& message) {
    errorMessage_ = message;
    state_ = ExecutionState::Error;
    stateChanged_.notify_all();
}

// Visitor implementations
void Interpreter::visit(IntegerLiteral& node) {
    currentValue_ = Value(node.value);
}

void Interpreter::visit(FloatLiteral& node) {
    currentValue_ = Value(node.value);
}

void Interpreter::visit(StringLiteral& node) {
    currentValue_ = Value(node.value);
}

void Interpreter::visit(Identifier& node) {
    currentValue_ = getVariable(node.name);
}

void Interpreter::visit(BooleanLiteral& node) {
    currentValue_ = Value(node.value);
}

void Interpreter::visit(UnaryExpr& node) {
    Value operand = evaluateExpression(*node.operand);

    switch (node.op) {
        case TokenType::NOT:
            currentValue_ = Value(!operand.isTruthy());
            break;
        case TokenType::MINUS:
            if (operand.isInt()) {
                currentValue_ = Value(-operand.asInt());
            } else {
                currentValue_ = Value(-operand.asNumber());
            }
            break;
        default:
            setError("Unknown unary operator");
            currentValue_ = Value();
    }
}

void Interpreter::visit(BinaryExpr& node) {
    Value left = evaluateExpression(*node.left);
    Value right = evaluateExpression(*node.right);

    switch (node.op) {
        case TokenType::PLUS:
            if (left.isString() || right.isString()) {
                currentValue_ = Value(left.toString() + right.toString());
            } else if (left.isInt() && right.isInt()) {
                currentValue_ = Value(left.asInt() + right.asInt());
            } else {
                currentValue_ = Value(left.asNumber() + right.asNumber());
            }
            break;

        case TokenType::MINUS:
            if (left.isInt() && right.isInt()) {
                currentValue_ = Value(left.asInt() - right.asInt());
            } else {
                currentValue_ = Value(left.asNumber() - right.asNumber());
            }
            break;

        case TokenType::STAR:
            if (left.isInt() && right.isInt()) {
                currentValue_ = Value(left.asInt() * right.asInt());
            } else {
                currentValue_ = Value(left.asNumber() * right.asNumber());
            }
            break;

        case TokenType::SLASH:
            if (right.asNumber() == 0.0) {
                setError("Division by zero");
                currentValue_ = Value();
            } else if (left.isInt() && right.isInt()) {
                currentValue_ = Value(left.asInt() / right.asInt());
            } else {
                currentValue_ = Value(left.asNumber() / right.asNumber());
            }
            break;

        case TokenType::EQUALS:
            currentValue_ = Value(left.toString() == right.toString());
            break;

        case TokenType::NOT_EQUALS:
            currentValue_ = Value(left.toString() != right.toString());
            break;

        case TokenType::LESS:
            currentValue_ = Value(left.asNumber() < right.asNumber());
            break;

        case TokenType::GREATER:
            currentValue_ = Value(left.asNumber() > right.asNumber());
            break;

        case TokenType::LESS_EQUAL:
            currentValue_ = Value(left.asNumber() <= right.asNumber());
            break;

        case TokenType::GREATER_EQUAL:
            currentValue_ = Value(left.asNumber() >= right.asNumber());
            break;

        case TokenType::AND:
            currentValue_ = Value(left.isTruthy() && right.isTruthy());
            break;

        case TokenType::OR:
            currentValue_ = Value(left.isTruthy() || right.isTruthy());
            break;

        default:
            setError("Unknown binary operator");
            currentValue_ = Value();
    }
}

void Interpreter::visit(NaturalExpr& node) {
    // Natural expressions need pattern matching
    if (patternMatcher_ && !node.tokens.empty()) {
        auto result = patternMatcher_->matchStatement(node.tokens, 0);
        if (result && result->pattern && result->pattern->definition) {
            // Found a matching pattern - create a PatternCall and execute it
            PatternCall call;
            call.pattern = result->pattern->definition;
            call.bindings = std::move(result->bindings);
            call.location = node.location;

            // Execute the pattern call
            visit(call);
            return;
        }
    }

    // No pattern matched
    setError("No matching pattern found for natural expression");
    currentValue_ = Value();
}

void Interpreter::visit(LazyExpr& node) {
    // When a LazyExpr is evaluated directly (not via evaluate intrinsic),
    // we evaluate its inner expression
    if (node.inner) {
        currentValue_ = evaluateExpression(*node.inner);
    } else {
        currentValue_ = Value();
    }
}

void Interpreter::visit(BlockExpr& node) {
    // When a BlockExpr is evaluated directly (not via execute intrinsic),
    // we execute all statements in the block
    for (auto& stmt : node.statements) {
        executeStatement(*stmt);
        if (state_ != ExecutionState::Running) break;
    }
    currentValue_ = Value();
}

void Interpreter::visit(ExpressionStmt& node) {
    if (node.expression) {
        evaluateExpression(*node.expression);
    }
}

void Interpreter::visit(SetStatement& node) {
    Value val = evaluateExpression(*node.value);

    // Store in current scope
    if (currentLocals_) {
        (*currentLocals_)[node.variable] = val;
    } else {
        globals_[node.variable] = val;
    }
}

void Interpreter::visit(IfStatement& node) {
    Value condition = evaluateExpression(*node.condition);

    if (condition.isTruthy()) {
        for (auto& stmt : node.then_branch) {
            executeStatement(*stmt);
            if (state_ != ExecutionState::Running) break;
        }
    } else {
        for (auto& stmt : node.else_branch) {
            executeStatement(*stmt);
            if (state_ != ExecutionState::Running) break;
        }
    }
}

void Interpreter::visit(WhileStatement& node) {
    while (state_ == ExecutionState::Running) {
        Value condition = evaluateExpression(*node.condition);
        if (!condition.isTruthy()) break;

        for (auto& stmt : node.body) {
            executeStatement(*stmt);
            if (state_ != ExecutionState::Running) break;
        }
    }
}

void Interpreter::visit(FunctionDecl& node) {
    // Functions are collected in the first pass
    functions_[node.name] = &node;
}

void Interpreter::visit(IntrinsicCall& node) {
    // Handle execute and evaluate specially - they need access to AST nodes
    if (node.name == "execute") {
        // @intrinsic("execute", block) - Execute a captured code block
        if (node.args.size() >= 1) {
            // Resolve deferred binding (may be an Identifier referring to a BlockExpr)
            Expression* resolved = resolveDeferredBinding(node.args[0].get());
            auto* blockExpr = dynamic_cast<BlockExpr*>(resolved);
            if (blockExpr) {
                // Execute all statements in the block
                for (auto& stmt : blockExpr->statements) {
                    executeStatement(*stmt);
                    if (hasReturnValue_ || state_ != ExecutionState::Running) break;
                }
            } else {
                // If not a BlockExpr, just evaluate the expression
                evaluateExpression(*resolved);
            }
        }
        currentValue_ = Value();
        return;
    }

    if (node.name == "evaluate") {
        // @intrinsic("evaluate", expr) - Evaluate a lazy expression
        if (node.args.size() >= 1) {
            // Resolve deferred binding (may be an Identifier referring to a LazyExpr)
            Expression* resolved = resolveDeferredBinding(node.args[0].get());
            auto* lazyExpr = dynamic_cast<LazyExpr*>(resolved);
            if (lazyExpr && lazyExpr->inner) {
                // Evaluate the inner expression
                currentValue_ = evaluateExpression(*lazyExpr->inner);
            } else {
                // Not a LazyExpr, just evaluate directly
                currentValue_ = evaluateExpression(*resolved);
            }
        } else {
            currentValue_ = Value();
        }
        return;
    }

    if (node.name == "execute_if") {
        // @intrinsic("execute_if", condition, block) - Execute block only if condition is true
        if (node.args.size() >= 2) {
            // Resolve deferred bindings
            Expression* condArg = resolveDeferredBinding(node.args[0].get());
            Expression* blockArg = resolveDeferredBinding(node.args[1].get());

            // Evaluate the condition (may be lazy)
            Value cond;
            auto* lazyExpr = dynamic_cast<LazyExpr*>(condArg);
            if (lazyExpr && lazyExpr->inner) {
                cond = evaluateExpression(*lazyExpr->inner);
            } else {
                cond = evaluateExpression(*condArg);
            }

            if (cond.isTruthy()) {
                // Execute the block
                auto* blockExpr = dynamic_cast<BlockExpr*>(blockArg);
                if (blockExpr) {
                    for (auto& stmt : blockExpr->statements) {
                        executeStatement(*stmt);
                        if (hasReturnValue_ || state_ != ExecutionState::Running) break;
                    }
                } else {
                    evaluateExpression(*blockArg);
                }
            }
        }
        currentValue_ = Value();
        return;
    }

    if (node.name == "loop_while") {
        // @intrinsic("loop_while", condition, block) - Loop while condition is true
        if (node.args.size() >= 2) {
            // Resolve deferred bindings
            Expression* condArg = resolveDeferredBinding(node.args[0].get());
            Expression* blockArg = resolveDeferredBinding(node.args[1].get());

            while (state_ == ExecutionState::Running && !hasReturnValue_) {
                // Evaluate the condition (may be lazy)
                Value cond;
                auto* lazyExpr = dynamic_cast<LazyExpr*>(condArg);
                if (lazyExpr && lazyExpr->inner) {
                    cond = evaluateExpression(*lazyExpr->inner);
                } else {
                    cond = evaluateExpression(*condArg);
                }

                if (!cond.isTruthy()) break;

                // Execute the block
                auto* blockExpr = dynamic_cast<BlockExpr*>(blockArg);
                if (blockExpr) {
                    for (auto& stmt : blockExpr->statements) {
                        executeStatement(*stmt);
                        if (hasReturnValue_ || state_ != ExecutionState::Running) break;
                    }
                } else {
                    evaluateExpression(*blockArg);
                }
            }
        }
        currentValue_ = Value();
        return;
    }

    // For other intrinsics, evaluate arguments first
    std::vector<Value> args;
    for (auto& argExpr : node.args) {
        args.push_back(evaluateExpression(*argExpr));
    }

    currentValue_ = handleIntrinsic(node.name, args);
}

void Interpreter::visit(PatternDef& node) {
    // Pattern definitions are handled at compile time
}

void Interpreter::visit(PatternCall& node) {
    // Execute the pattern's triggered behavior
    if (!node.pattern) {
        currentValue_ = Value();
        return;
    }

    // Set up bindings as local variables
    std::unordered_map<std::string, Value> patternLocals;
    auto* savedLocals = currentLocals_;
    auto savedDeferredBindings = deferredBindings_;

    // Save return state - patterns can nest
    bool savedHasReturnValue = hasReturnValue_;
    Value savedReturnValue = returnValue_;
    hasReturnValue_ = false;
    returnValue_ = Value();

    // Store BlockExpr and LazyExpr bindings for deferred access
    // These should not be evaluated until explicitly requested via intrinsics
    for (auto& binding : node.bindings) {
        if (dynamic_cast<BlockExpr*>(binding.second.get()) ||
            dynamic_cast<LazyExpr*>(binding.second.get())) {
            deferredBindings_[binding.first] = binding.second.get();
        } else {
            patternLocals[binding.first] = evaluateExpression(*binding.second);
        }
    }

    currentLocals_ = &patternLocals;

    // Execute triggered statements
    for (auto& stmt : node.pattern->when_triggered) {
        executeStatement(*stmt);
        // Check for early return from pattern
        if (hasReturnValue_ || state_ != ExecutionState::Running) break;
    }

    // Determine the result value
    if (hasReturnValue_) {
        // Return value was set via @intrinsic("return", val)
        currentValue_ = returnValue_;
    } else {
        // Check for 'result' variable (used by expression patterns)
        auto resultIt = patternLocals.find("result");
        if (resultIt != patternLocals.end()) {
            currentValue_ = resultIt->second;
        } else {
            currentValue_ = Value();
        }
    }

    // Restore state
    currentLocals_ = savedLocals;
    deferredBindings_ = savedDeferredBindings;
    hasReturnValue_ = savedHasReturnValue;
    returnValue_ = savedReturnValue;
}

void Interpreter::visit(ImportStmt& node) {
    // Imports are handled at parse time
}

void Interpreter::visit(UseStmt& node) {
    // Use statements are handled at parse time
}

void Interpreter::visit(ImportFunctionDecl& node) {
    // FFI functions not supported in interpreter
}

void Interpreter::visit(Program& node) {
    for (auto& stmt : node.statements) {
        executeStatement(*stmt);
        if (state_ != ExecutionState::Running) break;
    }
}

Value Interpreter::handleIntrinsic(const std::string& name, const std::vector<Value>& args) {
    if (name == "print") {
        if (!args.empty()) {
            output(args[0].toString() + "\n");
        }
        return Value();
    }

    if (name == "store") {
        if (args.size() >= 2 && args[0].isString()) {
            std::string varName = args[0].asString();
            if (currentLocals_) {
                (*currentLocals_)[varName] = args[1];
            } else {
                globals_[varName] = args[1];
            }
        }
        return Value();
    }

    if (name == "load") {
        if (!args.empty() && args[0].isString()) {
            return getVariable(args[0].asString());
        }
        return Value();
    }

    if (name == "add") {
        if (args.size() >= 2) {
            if (args[0].isInt() && args[1].isInt()) {
                return Value(args[0].asInt() + args[1].asInt());
            }
            return Value(args[0].asNumber() + args[1].asNumber());
        }
        return Value();
    }

    if (name == "sub") {
        if (args.size() >= 2) {
            if (args[0].isInt() && args[1].isInt()) {
                return Value(args[0].asInt() - args[1].asInt());
            }
            return Value(args[0].asNumber() - args[1].asNumber());
        }
        return Value();
    }

    if (name == "mul") {
        if (args.size() >= 2) {
            if (args[0].isInt() && args[1].isInt()) {
                return Value(args[0].asInt() * args[1].asInt());
            }
            return Value(args[0].asNumber() * args[1].asNumber());
        }
        return Value();
    }

    if (name == "div") {
        if (args.size() >= 2) {
            double divisor = args[1].asNumber();
            if (divisor == 0.0) {
                setError("Division by zero");
                return Value();
            }
            if (args[0].isInt() && args[1].isInt()) {
                return Value(args[0].asInt() / args[1].asInt());
            }
            return Value(args[0].asNumber() / divisor);
        }
        return Value();
    }

    // Note: "execute" and "evaluate" intrinsics require access to the AST nodes,
    // not just evaluated values. They are handled specially in visit(IntrinsicCall&).

    // Return intrinsic - sets return value and signals early return
    if (name == "return") {
        if (!args.empty()) {
            returnValue_ = args[0];
        } else {
            returnValue_ = Value();
        }
        hasReturnValue_ = true;
        return returnValue_;
    }

    // Comparison intrinsics
    if (name == "cmp_eq") {
        if (args.size() >= 2) {
            if (args[0].isString() || args[1].isString()) {
                return Value(args[0].toString() == args[1].toString());
            }
            return Value(args[0].asNumber() == args[1].asNumber());
        }
        return Value(false);
    }

    if (name == "cmp_neq") {
        if (args.size() >= 2) {
            if (args[0].isString() || args[1].isString()) {
                return Value(args[0].toString() != args[1].toString());
            }
            return Value(args[0].asNumber() != args[1].asNumber());
        }
        return Value(false);
    }

    if (name == "cmp_lt") {
        if (args.size() >= 2) {
            return Value(args[0].asNumber() < args[1].asNumber());
        }
        return Value(false);
    }

    if (name == "cmp_gt") {
        if (args.size() >= 2) {
            return Value(args[0].asNumber() > args[1].asNumber());
        }
        return Value(false);
    }

    if (name == "cmp_lte") {
        if (args.size() >= 2) {
            return Value(args[0].asNumber() <= args[1].asNumber());
        }
        return Value(false);
    }

    if (name == "cmp_gte") {
        if (args.size() >= 2) {
            return Value(args[0].asNumber() >= args[1].asNumber());
        }
        return Value(false);
    }

    // Unknown intrinsic - don't error, just return null (allows graceful fallback)
    return Value();
}

// Helper to resolve deferred bindings (BlockExpr/LazyExpr parameters)
Expression* Interpreter::resolveDeferredBinding(Expression* arg) {
    auto* id = dynamic_cast<Identifier*>(arg);
    if (id) {
        auto it = deferredBindings_.find(id->name);
        if (it != deferredBindings_.end()) {
            return it->second;
        }
    }
    return arg;
}

} // namespace tbx
