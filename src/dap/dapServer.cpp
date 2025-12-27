#include "dap/dapServer.hpp"
#include "interpreter/interpreter.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "pattern/pattern_registry.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

namespace tbx {

DapServer::DapServer() : interpreter_(std::make_unique<Interpreter>()) {}
DapServer::~DapServer() = default;

void DapServer::run() {
    log("3BX Debug Adapter starting...");

    while (true) {
        try {
            std::string message = readMessage();
            if (message.empty()) {
                break;
            }

            JsonValue json = JsonValue::parse(message);
            std::string type = json["type"].asString();

            if (type == "request") {
                std::string command = json["command"].asString();
                int seq = json["seq"].asInt();
                JsonValue args = json["arguments"];

                JsonValue result = handleRequest(command, args, seq);

                if (command == "disconnect") {
                    break;
                }
            }
        } catch (const std::exception& e) {
            log("Error: " + std::string(e.what()));
        }
    }

    log("3BX Debug Adapter shutting down.");
}

std::string DapServer::readMessage() {
    // Read headers until empty line
    int contentLength = -1;

    while (true) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            return ""; // EOF
        }

        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            break; // End of headers
        }

        // Parse Content-Length header
        if (line.find("Content-Length:") == 0) {
            contentLength = std::stoi(line.substr(15));
        }
    }

    if (contentLength <= 0) {
        log("Invalid Content-Length");
        return "";
    }

    // Read content
    std::string content(contentLength, '\0');
    std::cin.read(&content[0], contentLength);

    if (std::cin.gcount() != contentLength) {
        log("Failed to read full message content");
        return "";
    }

    log("Received: " + content);
    return content;
}

void DapServer::writeMessage(const std::string& content) {
    log("Sending: " + content);
    std::cout << "Content-Length: " << content.size() << "\r\n\r\n" << content;
    std::cout.flush();
}

void DapServer::sendResponse(int requestSeq, bool success, const std::string& command,
                             const JsonValue& body, const std::string& message) {
    JsonValue response = JsonValue::object();
    response.set("seq", sequenceNumber_++);
    response.set("type", "response");
    response.set("request_seq", requestSeq);
    response.set("success", success);
    response.set("command", command);

    if (!body.isNull()) {
        response.set("body", body);
    }

    if (!message.empty()) {
        response.set("message", message);
    }

    writeMessage(response.serialize());
}

void DapServer::sendEvent(const std::string& event, const JsonValue& body) {
    JsonValue eventMsg = JsonValue::object();
    eventMsg.set("seq", sequenceNumber_++);
    eventMsg.set("type", "event");
    eventMsg.set("event", event);

    if (!body.isNull()) {
        eventMsg.set("body", body);
    }

    writeMessage(eventMsg.serialize());
}

void DapServer::sendStoppedEvent(const std::string& reason, const std::string& description) {
    JsonValue body = JsonValue::object();
    body.set("reason", reason);
    body.set("threadId", 1);
    body.set("allThreadsStopped", true);

    if (!description.empty()) {
        body.set("description", description);
    }

    sendEvent("stopped", body);
}

void DapServer::sendTerminatedEvent() {
    sendEvent("terminated");
}

void DapServer::sendOutputEvent(const std::string& category, const std::string& output) {
    JsonValue body = JsonValue::object();
    body.set("category", category);
    body.set("output", output);
    sendEvent("output", body);
}

void DapServer::log(const std::string& message) {
    if (debug_) {
        std::cerr << "[3BX-DAP] " << message << std::endl;
    }
}

JsonValue DapServer::handleRequest(const std::string& command, const JsonValue& args, int seq) {
    log("Request: " + command);

    if (command == "initialize") {
        JsonValue result = handleInitialize(args);
        sendResponse(seq, true, command, result);

        // Send initialized event
        sendEvent("initialized");
        return result;
    }

    if (command == "launch") {
        JsonValue result = handleLaunch(args);
        sendResponse(seq, !result.isNull() || launched_, command, result,
                     launched_ ? "" : "Launch failed");
        return result;
    }

    if (command == "setBreakpoints") {
        JsonValue result = handleSetBreakpoints(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "configurationDone") {
        JsonValue result = handleConfigurationDone(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "threads") {
        JsonValue result = handleThreads(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "stackTrace") {
        JsonValue result = handleStackTrace(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "scopes") {
        JsonValue result = handleScopes(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "variables") {
        JsonValue result = handleVariables(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "continue") {
        JsonValue result = handleContinue(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "next") {
        JsonValue result = handleNext(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "stepIn") {
        JsonValue result = handleStepIn(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "stepOut") {
        JsonValue result = handleStepOut(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "pause") {
        JsonValue result = handlePause(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "disconnect") {
        JsonValue result = handleDisconnect(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    if (command == "evaluate") {
        JsonValue result = handleEvaluate(args);
        sendResponse(seq, true, command, result);
        return result;
    }

    // Unknown command
    log("Unknown command: " + command);
    sendResponse(seq, false, command, JsonValue(), "Unknown command: " + command);
    return JsonValue();
}

JsonValue DapServer::handleInitialize(const JsonValue& args) {
    initialized_ = true;

    // Return capabilities
    JsonValue capabilities = JsonValue::object();
    capabilities.set("supportsConfigurationDoneRequest", true);
    capabilities.set("supportsFunctionBreakpoints", false);
    capabilities.set("supportsConditionalBreakpoints", false);
    capabilities.set("supportsHitConditionalBreakpoints", false);
    capabilities.set("supportsEvaluateForHovers", true);
    capabilities.set("supportsStepBack", false);
    capabilities.set("supportsSetVariable", false);
    capabilities.set("supportsRestartFrame", false);
    capabilities.set("supportsGotoTargetsRequest", false);
    capabilities.set("supportsStepInTargetsRequest", false);
    capabilities.set("supportsCompletionsRequest", false);
    capabilities.set("supportsModulesRequest", false);
    capabilities.set("supportsExceptionOptions", false);
    capabilities.set("supportsValueFormattingOptions", false);
    capabilities.set("supportsExceptionInfoRequest", false);
    capabilities.set("supportTerminateDebuggee", true);
    capabilities.set("supportsDelayedStackTraceLoading", false);
    capabilities.set("supportsLoadedSourcesRequest", false);
    capabilities.set("supportsLogPoints", false);
    capabilities.set("supportsTerminateThreadsRequest", false);
    capabilities.set("supportsSetExpression", false);
    capabilities.set("supportsTerminateRequest", true);

    return capabilities;
}

JsonValue DapServer::handleLaunch(const JsonValue& args) {
    sourceFile_ = args["program"].asString();

    if (sourceFile_.empty()) {
        sendOutputEvent("stderr", "Error: No program specified in launch configuration\n");
        return JsonValue();
    }

    // Read source file
    std::ifstream file(sourceFile_);
    if (!file) {
        sendOutputEvent("stderr", "Error: Cannot open file: " + sourceFile_ + "\n");
        return JsonValue();
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    sourceContent_ = buffer.str();

    sendOutputEvent("console", "Loading: " + sourceFile_ + "\n");

    try {
        // Create pattern registry (owned by DapServer for lifetime)
        patternRegistry_ = std::make_unique<PatternRegistry>();

        // Parse the program
        Lexer lexer(sourceContent_, sourceFile_);
        Parser parser(lexer);
        parser.setSharedRegistry(patternRegistry_.get());

        auto program = parser.parse();

        // Load into interpreter with pattern registry for pattern matching
        interpreter_->load(program.release());
        interpreter_->setPatternRegistry(patternRegistry_.get());

        // Set up callbacks
        interpreter_->setBreakpointCallback([this](const SourceLocation& loc) {
            return shouldBreak(loc);
        });

        interpreter_->setOutputCallback([this](const std::string& text) {
            sendOutputEvent("stdout", text);
        });

        launched_ = true;
        state_ = DebugState::Paused;

        sendOutputEvent("console", "Program loaded successfully\n");

    } catch (const std::exception& e) {
        sendOutputEvent("stderr", "Error loading program: " + std::string(e.what()) + "\n");
        return JsonValue();
    }

    return JsonValue::object();
}

JsonValue DapServer::handleSetBreakpoints(const JsonValue& args) {
    JsonValue source = args["source"];
    std::string path = source["path"].asString();

    // Clear existing breakpoints for this source
    breakpoints_[path].clear();

    JsonValue result = JsonValue::object();
    JsonValue breakpointsArray = JsonValue::array();

    if (args.has("breakpoints")) {
        const auto& bps = args["breakpoints"].asArray();
        for (const auto& bp : bps) {
            int line = bp["line"].asInt();

            Breakpoint newBp;
            newBp.id = nextBreakpointId_++;
            newBp.source = path;
            newBp.line = line;
            newBp.verified = true;

            breakpoints_[path].push_back(newBp);

            JsonValue bpResult = JsonValue::object();
            bpResult.set("id", newBp.id);
            bpResult.set("verified", true);
            bpResult.set("line", line);
            breakpointsArray.push(bpResult);
        }
    }

    result.set("breakpoints", breakpointsArray);
    return result;
}

JsonValue DapServer::handleConfigurationDone(const JsonValue& args) {
    // Start execution in a separate thread
    if (launched_) {
        std::thread executionThread([this]() {
            startExecution();
        });
        executionThread.detach();
    }

    return JsonValue::object();
}

JsonValue DapServer::handleThreads(const JsonValue& args) {
    JsonValue result = JsonValue::object();
    JsonValue threads = JsonValue::array();

    JsonValue thread = JsonValue::object();
    thread.set("id", 1);
    thread.set("name", "main");
    threads.push(thread);

    result.set("threads", threads);
    return result;
}

JsonValue DapServer::handleStackTrace(const JsonValue& args) {
    JsonValue result = JsonValue::object();
    JsonValue stackFrames = JsonValue::array();

    const auto& callStack = interpreter_->callStack();
    int frameId = 1;

    for (auto it = callStack.rbegin(); it != callStack.rend(); ++it) {
        JsonValue frame = JsonValue::object();
        frame.set("id", frameId++);
        frame.set("name", it->name);
        frame.set("line", (int)it->location.line);
        frame.set("column", (int)it->location.column);

        JsonValue source = JsonValue::object();
        source.set("name", it->location.filename);
        source.set("path", it->location.filename);
        frame.set("source", source);

        stackFrames.push(frame);
    }

    result.set("stackFrames", stackFrames);
    result.set("totalFrames", (int)callStack.size());
    return result;
}

JsonValue DapServer::handleScopes(const JsonValue& args) {
    int frameId = args["frameId"].asInt();

    JsonValue result = JsonValue::object();
    JsonValue scopes = JsonValue::array();

    // Locals scope
    JsonValue locals = JsonValue::object();
    locals.set("name", "Locals");
    locals.set("variablesReference", 1);
    locals.set("expensive", false);
    scopes.push(locals);

    // Globals scope
    JsonValue globals = JsonValue::object();
    globals.set("name", "Globals");
    globals.set("variablesReference", 2);
    globals.set("expensive", false);
    scopes.push(globals);

    result.set("scopes", scopes);
    return result;
}

JsonValue DapServer::handleVariables(const JsonValue& args) {
    int variablesRef = args["variablesReference"].asInt();

    JsonValue result = JsonValue::object();
    JsonValue variables = JsonValue::array();

    std::vector<std::pair<std::string, Value>> vars;

    if (variablesRef == 1) {
        vars = interpreter_->getLocals();
    } else if (variablesRef == 2) {
        vars = interpreter_->getGlobals();
    }

    for (const auto& varPair : vars) {
        JsonValue var = JsonValue::object();
        var.set("name", varPair.first);
        var.set("value", varPair.second.toString());
        var.set("type", varPair.second.typeName());
        var.set("variablesReference", 0);
        variables.push(var);
    }

    result.set("variables", variables);
    return result;
}

JsonValue DapServer::handleContinue(const JsonValue& args) {
    continueExecution();

    JsonValue result = JsonValue::object();
    result.set("allThreadsContinued", true);
    return result;
}

JsonValue DapServer::handleNext(const JsonValue& args) {
    stepExecution(StepType::Over);

    JsonValue result = JsonValue::object();
    return result;
}

JsonValue DapServer::handleStepIn(const JsonValue& args) {
    stepExecution(StepType::In);

    JsonValue result = JsonValue::object();
    return result;
}

JsonValue DapServer::handleStepOut(const JsonValue& args) {
    stepExecution(StepType::Out);

    JsonValue result = JsonValue::object();
    return result;
}

JsonValue DapServer::handlePause(const JsonValue& args) {
    pauseExecution();
    return JsonValue::object();
}

JsonValue DapServer::handleDisconnect(const JsonValue& args) {
    interpreter_->stop();
    return JsonValue::object();
}

JsonValue DapServer::handleEvaluate(const JsonValue& args) {
    std::string expression = args["expression"].asString();

    Value value = interpreter_->evaluate(expression);

    JsonValue result = JsonValue::object();
    result.set("result", value.toString());
    result.set("type", value.typeName());
    result.set("variablesReference", 0);
    return result;
}

void DapServer::startExecution() {
    state_ = DebugState::Running;

    interpreter_->run();

    // Check result
    if (interpreter_->state() == ExecutionState::Error) {
        sendOutputEvent("stderr", "Error: " + interpreter_->errorMessage() + "\n");
    }

    sendTerminatedEvent();
}

void DapServer::pauseExecution() {
    interpreter_->pause();
    state_ = DebugState::Paused;
    sendStoppedEvent("pause", "Paused");
}

void DapServer::continueExecution() {
    state_ = DebugState::Running;
    interpreter_->resume();
}

void DapServer::stepExecution(StepType type) {
    state_ = DebugState::Stepping;
    stepType_ = type;

    StepMode mode = StepMode::None;
    switch (type) {
        case StepType::In:
            mode = StepMode::In;
            break;
        case StepType::Over:
            mode = StepMode::Over;
            break;
        case StepType::Out:
            mode = StepMode::Out;
            break;
        default:
            break;
    }

    interpreter_->step(mode);
}

bool DapServer::shouldBreak(const SourceLocation& location) {
    // Check breakpoints
    auto it = breakpoints_.find(location.filename);
    if (it != breakpoints_.end()) {
        for (const auto& bp : it->second) {
            if (bp.line == (int)location.line) {
                state_ = DebugState::Paused;
                sendStoppedEvent("breakpoint", "Breakpoint hit");
                return true;
            }
        }
    }

    // Check if stepping should stop
    if (state_ == DebugState::Stepping) {
        state_ = DebugState::Paused;
        sendStoppedEvent("step", "Step completed");
        return true;
    }

    return false;
}

} // namespace tbx
