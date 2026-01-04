#pragma once

#include "compiler/diagnostic.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include "semanticTokenTypes.hpp"
#include "semanticTokensBuilder.hpp"

namespace tbx {

using json = nlohmann::json;

// Forward declarations
class Lexer;

// Represents a position in a document (0-indexed)
struct LspPosition {
    int line{};
    int character{};
};

// Represents a range in a document
struct LspRange {
    LspPosition start;
    LspPosition end;
};

// Represents a diagnostic (error/warning)
struct LspDiagnostic {
    LspRange range;
    int severity{1}; // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string message;
    std::string source{"3bx"};
};

// Represents an open document
struct TextDocument {
    std::string uri;
    std::string content;
    int version{};
};

// Represents a location in source code for go-to-definition
struct LspLocation {
    std::string uri;
    LspRange range;
};

// Represents a pattern definition with its location
struct PatternDefLocation {
    std::string syntax;              // The pattern syntax string (e.g., "set var to val")
    std::vector<std::string> words;  // Words in the pattern (for matching)
    LspLocation location;            // Where the pattern is defined
    LspRange usageRange;             // Where the pattern is USED (added for resolved Go-to-Definition)
    bool isPrivate = false;          // Whether the pattern is private
};

// LSP Server implementation
class LspServer {
public:
    LspServer();
    ~LspServer();

    // Run the server (main loop reading from stdin, writing to stdout)
    void run();

    // Process a single message (for testing)
    std::string processMessage(const std::string& message);

    // Enable debug mode (writes logs to stderr)
    void setDebug(bool debug) { debug_ = debug; }

private:
    bool debug_{};
    bool initialized_{};
    bool shutdown_{};
    std::unordered_map<std::string, TextDocument> documents_;

    // Pattern definitions indexed by document URI
    std::unordered_map<std::string, std::vector<PatternDefLocation>> patternDefinitions_;

    // Message handling
    json handleRequest(const std::string& method, const json& params, const json& id);
    void handleNotification(const std::string& method, const json& params);

    // LSP method handlers
    json handleInitialize(const json& params);
    void handleInitialized(const json& params);
    void handleShutdown();
    void handleExit();

    // Document sync
    void handleDidOpen(const json& params);
    void handleDidChange(const json& params);
    void handleDidClose(const json& params);

    // Language features
    json handleCompletion(const json& params);
    json handleHover(const json& params);
    json handleDefinition(const json& params);

    // Diagnostics
    void publishDiagnostics(const std::string& uri, const std::string& content);
    std::vector<LspDiagnostic> getDiagnostics(const std::string& content, const std::string& filename);

    // Semantic tokens
    json handleSemanticTokensFull(const json& params);
    std::vector<int32_t> computeSemanticTokens(const std::string& uri);

    // IO helpers
    std::string readMessage();
    void writeMessage(const std::string& content);
    void sendResponse(const json& id, const json& result);
    void sendError(const json& id, int code, const std::string& message);
    void sendNotification(const std::string& method, const json& params);

    // Logging
    void log(const std::string& message);
    void logToFile(const std::string& message);

    // Utility
    std::string uriToPath(const std::string& uri);
    std::string pathToUri(const std::string& path);

    // Pattern definition tracking
    void extractPatternDefinitions(const std::string& uri, const std::string& content);
    void processImports(const std::string& uri, const std::string& content);
    std::string resolveImportPath(const std::string& importPath, const std::string& sourceDir);
    PatternDefLocation* findPatternAtPosition(const std::string& uri, int line, int character);
};

} // namespace tbx
