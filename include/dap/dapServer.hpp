#pragma once

#include "lsp/lspServer.hpp"
#include "lexer/token.hpp"  // For SourceLocation
#include <nlohmann/json.hpp>

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

    // TODO: The interpreter has been removed from the old pipeline
    // Debugging will be re-implemented with the new compiler pipeline

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
    nlohmann::json handleRequest(const std::string& command, const nlohmann::json& args, int seq);

    // DAP request handlers
    nlohmann::json handleInitialize(const nlohmann::json& args);
    nlohmann::json handleLaunch(const nlohmann::json& args);
    nlohmann::json handleSetBreakpoints(const nlohmann::json& args);
    nlohmann::json handleConfigurationDone(const nlohmann::json& args);
    nlohmann::json handleThreads(const nlohmann::json& args);
    nlohmann::json handleStackTrace(const nlohmann::json& args);
    nlohmann::json handleScopes(const nlohmann::json& args);
    nlohmann::json handleVariables(const nlohmann::json& args);
    nlohmann::json handleContinue(const nlohmann::json& args);
    nlohmann::json handleNext(const nlohmann::json& args);
    nlohmann::json handleStepIn(const nlohmann::json& args);
    nlohmann::json handleStepOut(const nlohmann::json& args);
    nlohmann::json handlePause(const nlohmann::json& args);
    nlohmann::json handleDisconnect(const nlohmann::json& args);
    nlohmann::json handleEvaluate(const nlohmann::json& args);

    // Execution control
    void startExecution();
    void pauseExecution();
    void continueExecution();
    void stepExecution(StepType type);

    // Breakpoint checking
    bool shouldBreak(const SourceLocation& location);

    // Event sending
    void sendEvent(const std::string& event, const nlohmann::json& body = nlohmann::json());
    void sendStoppedEvent(const std::string& reason, const std::string& description = "");
    void sendTerminatedEvent();
    void sendOutputEvent(const std::string& category, const std::string& output);

    // IO helpers (reusing LSP JSON format)
    std::string readMessage();
    void writeMessage(const std::string& content);
    void sendResponse(int requestSeq, bool success, const std::string& command,
                      const nlohmann::json& body = nlohmann::json(), const std::string& message = "");

    // Logging
    void log(const std::string& message);
};

} // namespace tbx
