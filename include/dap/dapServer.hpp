#pragma once

#include "ast/ast.hpp"
#include "lsp/lspServer.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx {

// Forward declarations
class Interpreter;
class PatternRegistry;

// Breakpoint information
struct Breakpoint {
    int id{};
    std::string source;
    int line{};
    bool verified{};
};

// Stack frame information
struct StackFrame {
    int id{};
    std::string name;
    std::string source;
    int line{};
    int column{};
};

// Variable information
struct Variable {
    std::string name;
    std::string value;
    std::string type;
    int variablesReference{}; // 0 = no children, >0 = has children
};

// Scope information
struct Scope {
    std::string name;
    int variablesReference{};
    bool expensive{};
};

// Thread information (3BX is single-threaded for now)
struct ThreadInfo {
    int id{1};
    std::string name{"main"};
};

// Debug execution state
enum class DebugState {
    Stopped,      // Not running
    Running,      // Running normally
    Paused,       // Paused at breakpoint or step
    Stepping      // Executing a step operation
};

// Step type
enum class StepType {
    None,
    In,
    Over,
    Out
};

// Debug Adapter Protocol server implementation
class DapServer {
public:
    DapServer();
    ~DapServer();

    // Run the DAP server (main loop reading from stdin, writing to stdout)
    void run();

    // Enable debug logging
    void setDebug(bool debug) { debug_ = debug; }

private:
    bool debug_{};
    bool initialized_{};
    bool launched_{};
    int sequenceNumber_{1};

    // Interpreter for executing 3BX code
    std::unique_ptr<Interpreter> interpreter_;

    // Pattern registry (owned by DapServer for lifetime management)
    std::unique_ptr<PatternRegistry> patternRegistry_;

    // Breakpoint management
    int nextBreakpointId_{1};
    std::unordered_map<std::string, std::vector<Breakpoint>> breakpoints_;

    // Stack frame management
    int nextFrameId_{1};
    std::vector<StackFrame> stackFrames_;

    // Variable references
    int nextVariableRef_{1};
    std::unordered_map<int, std::vector<Variable>> variableRefs_;

    // Execution state
    std::atomic<DebugState> state_{DebugState::Stopped};
    StepType stepType_{StepType::None};
    int stepDepth_{};
    std::mutex stateMutex_;
    std::condition_variable stateChanged_;

    // Source file content
    std::string sourceFile_;
    std::string sourceContent_;

    // Message handling
    JsonValue handleRequest(const std::string& command, const JsonValue& args, int seq);

    // DAP request handlers
    JsonValue handleInitialize(const JsonValue& args);
    JsonValue handleLaunch(const JsonValue& args);
    JsonValue handleSetBreakpoints(const JsonValue& args);
    JsonValue handleConfigurationDone(const JsonValue& args);
    JsonValue handleThreads(const JsonValue& args);
    JsonValue handleStackTrace(const JsonValue& args);
    JsonValue handleScopes(const JsonValue& args);
    JsonValue handleVariables(const JsonValue& args);
    JsonValue handleContinue(const JsonValue& args);
    JsonValue handleNext(const JsonValue& args);
    JsonValue handleStepIn(const JsonValue& args);
    JsonValue handleStepOut(const JsonValue& args);
    JsonValue handlePause(const JsonValue& args);
    JsonValue handleDisconnect(const JsonValue& args);
    JsonValue handleEvaluate(const JsonValue& args);

    // Execution control
    void startExecution();
    void pauseExecution();
    void continueExecution();
    void stepExecution(StepType type);

    // Breakpoint checking
    bool shouldBreak(const SourceLocation& location);

    // Event sending
    void sendEvent(const std::string& event, const JsonValue& body = JsonValue());
    void sendStoppedEvent(const std::string& reason, const std::string& description = "");
    void sendTerminatedEvent();
    void sendOutputEvent(const std::string& category, const std::string& output);

    // IO helpers (reusing LSP JSON format)
    std::string readMessage();
    void writeMessage(const std::string& content);
    void sendResponse(int requestSeq, bool success, const std::string& command,
                      const JsonValue& body = JsonValue(), const std::string& message = "");

    // Logging
    void log(const std::string& message);
};

} // namespace tbx
