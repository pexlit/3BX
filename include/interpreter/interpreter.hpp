#pragma once

#include "ast/ast.hpp"
#include "lexer/token.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tbx {

// Forward declarations
class PatternRegistry;
class PatternMatcher;

// Runtime value type for the interpreter
struct Value {
    using ValueType = std::variant<
        std::monostate,  // null/undefined
        int64_t,         // integer
        double,          // float
        std::string,     // string
        bool             // boolean
    >;

    ValueType data;

    Value() : data(std::monostate{}) {}
    Value(int64_t v) : data(v) {}
    Value(double v) : data(v) {}
    Value(const std::string& v) : data(v) {}
    Value(const char* v) : data(std::string(v)) {}
    Value(bool v) : data(v) {}

    // Type checks
    bool isNull() const { return std::holds_alternative<std::monostate>(data); }
    bool isInt() const { return std::holds_alternative<int64_t>(data); }
    bool isFloat() const { return std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<std::string>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isNumeric() const { return isInt() || isFloat(); }

    // Value accessors
    int64_t asInt() const { return std::get<int64_t>(data); }
    double asFloat() const { return std::get<double>(data); }
    const std::string& asString() const { return std::get<std::string>(data); }
    bool asBool() const { return std::get<bool>(data); }

    // Get numeric value as double
    double asNumber() const {
        if (isInt()) return (double)asInt();
        if (isFloat()) return asFloat();
        return 0.0;
    }

    // Convert to string for display
    std::string toString() const;

    // Get type name
    std::string typeName() const;

    // Truthiness check
    bool isTruthy() const;
};

// Callback for debug events
using BreakpointCallback = std::function<bool(const SourceLocation&)>;
using OutputCallback = std::function<void(const std::string&)>;

// Execution state
enum class ExecutionState {
    Idle,       // Not running
    Running,    // Running normally
    Paused,     // Paused at breakpoint or step
    Finished,   // Execution complete
    Error       // Error occurred
};

// Step mode
enum class StepMode {
    None,
    In,
    Over,
    Out
};

// Call stack frame for debugging
struct CallFrame {
    std::string name;
    SourceLocation location;
    std::unordered_map<std::string, Value>* locals;
};

// Interpreter that executes AST nodes with debugging support
class Interpreter : public ASTVisitor {
public:
    Interpreter();
    ~Interpreter();

    // Load a program for execution
    void load(Program* program);

    // Set pattern registry for pattern matching support
    void setPatternRegistry(PatternRegistry* registry);

    // Execute the loaded program
    void run();

    // Execution control
    void pause();
    void resume();
    void step(StepMode mode);
    void stop();

    // State queries
    ExecutionState state() const { return state_; }
    bool isRunning() const { return state_ == ExecutionState::Running; }
    bool isPaused() const { return state_ == ExecutionState::Paused; }
    bool isFinished() const { return state_ == ExecutionState::Finished; }
    const std::string& errorMessage() const { return errorMessage_; }

    // Current location
    const SourceLocation& currentLocation() const { return currentLocation_; }

    // Call stack access
    const std::vector<CallFrame>& callStack() const { return callStack_; }

    // Variable access
    std::vector<std::pair<std::string, Value>> getLocals() const;
    std::vector<std::pair<std::string, Value>> getGlobals() const;
    Value getVariable(const std::string& name) const;

    // Evaluate an expression in current context
    Value evaluate(const std::string& expr);

    // Callbacks
    void setBreakpointCallback(BreakpointCallback cb) { breakpointCallback_ = std::move(cb); }
    void setOutputCallback(OutputCallback cb) { outputCallback_ = std::move(cb); }

    // Wait for state change
    void waitForStateChange(ExecutionState expected);

    // ASTVisitor implementation
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
    // Program being executed
    Program* program_{};

    // Execution state
    std::atomic<ExecutionState> state_{ExecutionState::Idle};
    std::string errorMessage_;
    mutable std::mutex stateMutex_;
    std::condition_variable stateChanged_;

    // Step control
    StepMode stepMode_{StepMode::None};
    int stepDepth_{};

    // Current location for debugging
    SourceLocation currentLocation_;

    // Call stack
    std::vector<CallFrame> callStack_;

    // Variable storage
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, Value>* currentLocals_{};

    // Function definitions
    std::unordered_map<std::string, FunctionDecl*> functions_;

    // Pattern matching support
    PatternRegistry* patternRegistry_{};
    std::unique_ptr<PatternMatcher> patternMatcher_;

    // Deferred bindings for pattern execution (BlockExpr and LazyExpr)
    std::unordered_map<std::string, Expression*> deferredBindings_;

    // Current expression result
    Value currentValue_;

    // Return value handling for pattern execution
    bool hasReturnValue_{false};
    Value returnValue_;

    // Callbacks
    BreakpointCallback breakpointCallback_;
    OutputCallback outputCallback_;

    // Helper methods
    void executeStatement(Statement& stmt);
    Value evaluateExpression(Expression& expr);
    void checkBreakpoint(const SourceLocation& location);
    void updateLocation(const ASTNode& node);
    void output(const std::string& text);
    void setError(const std::string& message);

    // Intrinsic handlers
    Value handleIntrinsic(const std::string& name, const std::vector<Value>& args);

    // Helper to resolve deferred bindings (BlockExpr/LazyExpr parameters)
    Expression* resolveDeferredBinding(Expression* arg);
};

} // namespace tbx
