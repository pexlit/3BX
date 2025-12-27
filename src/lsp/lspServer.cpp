#include "lsp/lspServer.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "pattern/pattern_registry.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace tbx {

// ============================================================================
// JsonValue implementation
// ============================================================================

JsonValue::JsonValue(std::initializer_list<std::pair<std::string, JsonValue>> obj)
    : type_(Type::Object) {
    for (const auto& pair : obj) {
        objectVal_[pair.first] = pair.second;
    }
}

JsonValue JsonValue::array() {
    JsonValue val;
    val.type_ = Type::Array;
    return val;
}

JsonValue JsonValue::object() {
    JsonValue val;
    val.type_ = Type::Object;
    return val;
}

void JsonValue::push(const JsonValue& val) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        arrayVal_.clear();
    }
    arrayVal_.push_back(val);
}

void JsonValue::set(const std::string& key, const JsonValue& val) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        objectVal_.clear();
    }
    objectVal_[key] = val;
}

bool JsonValue::has(const std::string& key) const {
    return type_ == Type::Object && objectVal_.count(key) > 0;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        objectVal_.clear();
    }
    return objectVal_[key];
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    static JsonValue null;
    if (type_ != Type::Object) return null;
    auto it = objectVal_.find(key);
    return it != objectVal_.end() ? it->second : null;
}

std::string JsonValue::serialize() const {
    std::ostringstream out;
    switch (type_) {
        case Type::Null:
            out << "null";
            break;
        case Type::Bool:
            out << (boolVal_ ? "true" : "false");
            break;
        case Type::Number:
            if (numberVal_ == (int)numberVal_) {
                out << (int)numberVal_;
            } else {
                out << numberVal_;
            }
            break;
        case Type::String: {
            out << '"';
            for (char c : stringVal_) {
                switch (c) {
                    case '"': out << "\\\""; break;
                    case '\\': out << "\\\\"; break;
                    case '\n': out << "\\n"; break;
                    case '\r': out << "\\r"; break;
                    case '\t': out << "\\t"; break;
                    default:
                        if (c >= 0 && c < 32) {
                            out << "\\u" << std::hex << std::setfill('0')
                                << std::setw(4) << (int)(unsigned char)c;
                        } else {
                            out << c;
                        }
                }
            }
            out << '"';
            break;
        }
        case Type::Array: {
            out << '[';
            bool first = true;
            for (const auto& val : arrayVal_) {
                if (!first) out << ',';
                first = false;
                out << val.serialize();
            }
            out << ']';
            break;
        }
        case Type::Object: {
            out << '{';
            bool first = true;
            for (const auto& pair : objectVal_) {
                if (!first) out << ',';
                first = false;
                out << '"' << pair.first << "\":" << pair.second.serialize();
            }
            out << '}';
            break;
        }
    }
    return out.str();
}

void JsonValue::skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace((unsigned char)json[pos])) {
        pos++;
    }
}

std::string JsonValue::parseString(const std::string& json, size_t& pos) {
    if (json[pos] != '"') {
        throw std::runtime_error("Expected string");
    }
    pos++; // skip opening quote

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            switch (json[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    // Parse unicode escape (simplified - just skip)
                    pos += 4;
                    result += '?';
                    break;
                }
                default: result += json[pos];
            }
        } else {
            result += json[pos];
        }
        pos++;
    }

    if (pos >= json.size()) {
        throw std::runtime_error("Unterminated string");
    }
    pos++; // skip closing quote
    return result;
}

double JsonValue::parseNumber(const std::string& json, size_t& pos) {
    size_t start = pos;
    if (json[pos] == '-') pos++;
    while (pos < json.size() && std::isdigit((unsigned char)json[pos])) pos++;
    if (pos < json.size() && json[pos] == '.') {
        pos++;
        while (pos < json.size() && std::isdigit((unsigned char)json[pos])) pos++;
    }
    if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
        pos++;
        if (json[pos] == '+' || json[pos] == '-') pos++;
        while (pos < json.size() && std::isdigit((unsigned char)json[pos])) pos++;
    }
    return std::stod(json.substr(start, pos - start));
}

JsonValue JsonValue::parseValue(const std::string& json, size_t& pos) {
    skipWhitespace(json, pos);

    if (pos >= json.size()) {
        return JsonValue();
    }

    char c = json[pos];

    if (c == 'n') {
        pos += 4; // null
        return JsonValue();
    }
    if (c == 't') {
        pos += 4; // true
        return JsonValue(true);
    }
    if (c == 'f') {
        pos += 5; // false
        return JsonValue(false);
    }
    if (c == '"') {
        return JsonValue(parseString(json, pos));
    }
    if (c == '-' || std::isdigit((unsigned char)c)) {
        return JsonValue(parseNumber(json, pos));
    }
    if (c == '[') {
        pos++; // skip [
        JsonValue arr = JsonValue::array();
        skipWhitespace(json, pos);
        if (json[pos] != ']') {
            while (true) {
                arr.push(parseValue(json, pos));
                skipWhitespace(json, pos);
                if (json[pos] == ']') break;
                if (json[pos] != ',') {
                    throw std::runtime_error("Expected ',' or ']' in array");
                }
                pos++; // skip ,
            }
        }
        pos++; // skip ]
        return arr;
    }
    if (c == '{') {
        pos++; // skip {
        JsonValue obj = JsonValue::object();
        skipWhitespace(json, pos);
        if (json[pos] != '}') {
            while (true) {
                skipWhitespace(json, pos);
                std::string key = parseString(json, pos);
                skipWhitespace(json, pos);
                if (json[pos] != ':') {
                    throw std::runtime_error("Expected ':' in object");
                }
                pos++; // skip :
                obj.set(key, parseValue(json, pos));
                skipWhitespace(json, pos);
                if (json[pos] == '}') break;
                if (json[pos] != ',') {
                    throw std::runtime_error("Expected ',' or '}' in object");
                }
                pos++; // skip ,
            }
        }
        pos++; // skip }
        return obj;
    }

    throw std::runtime_error(std::string("Unexpected character: ") + c);
}

JsonValue JsonValue::parse(const std::string& json) {
    size_t pos = 0;
    return parseValue(json, pos);
}

// ============================================================================
// LspServer implementation
// ============================================================================

LspServer::LspServer() : registry_(std::make_unique<PatternRegistry>()) {
    registry_->loadPrimitives();
    // Enable debug by default for now to help diagnose go-to-definition issues
    debug_ = true;
}

LspServer::~LspServer() = default;

void LspServer::run() {
    // Log immediately to stderr so we can confirm the server started
    std::cerr << "[3BX-LSP] Server starting... (debug=" << (debug_ ? "true" : "false") << ")" << std::endl;
    std::cerr.flush();

    log("3BX Language Server starting...");

    while (!shutdown_) {
        try {
            std::string message = readMessage();
            if (message.empty()) {
                break;
            }

            std::string response = processMessage(message);
            if (!response.empty()) {
                writeMessage(response);
            }
        } catch (const std::exception& e) {
            log("Error: " + std::string(e.what()));
        }
    }

    log("3BX Language Server shutting down.");
}

std::string LspServer::readMessage() {
    // Read headers until empty line
    std::string headers;
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

void LspServer::writeMessage(const std::string& content) {
    log("Sending: " + content);
    std::cout << "Content-Length: " << content.size() << "\r\n\r\n" << content;
    std::cout.flush();
}

std::string LspServer::processMessage(const std::string& message) {
    JsonValue json = JsonValue::parse(message);

    std::string method = json["method"].asString();
    JsonValue params = json["params"];

    // IMPORTANT: Check if "id" exists BEFORE accessing json["id"]
    // because operator[] on non-const JsonValue creates the key if it doesn't exist!
    bool hasId = json.has("id");
    JsonValue id;
    if (hasId) {
        id = json["id"];
    }

    if (hasId) {
        // This is a request
        JsonValue result = handleRequest(method, params, id);

        JsonValue response = JsonValue::object();
        response.set("jsonrpc", "2.0");
        response.set("id", id);
        response.set("result", result);
        return response.serialize();
    } else {
        // This is a notification
        handleNotification(method, params);
        return "";
    }
}

JsonValue LspServer::handleRequest(const std::string& method, const JsonValue& params, const JsonValue& id) {
    log("Request: " + method);

    if (method == "initialize") {
        return handleInitialize(params);
    }
    if (method == "shutdown") {
        handleShutdown();
        return JsonValue();
    }
    if (method == "textDocument/completion") {
        return handleCompletion(params);
    }
    if (method == "textDocument/hover") {
        return handleHover(params);
    }
    if (method == "textDocument/definition") {
        return handleDefinition(params);
    }

    // Unknown method
    log("Unknown request method: " + method);
    return JsonValue();
}

void LspServer::handleNotification(const std::string& method, const JsonValue& params) {
    log("Notification: " + method);

    if (method == "initialized") {
        handleInitialized(params);
    } else if (method == "exit") {
        handleExit();
    } else if (method == "textDocument/didOpen") {
        handleDidOpen(params);
    } else if (method == "textDocument/didChange") {
        handleDidChange(params);
    } else if (method == "textDocument/didClose") {
        handleDidClose(params);
    } else {
        log("Unknown notification method: " + method);
    }
}

JsonValue LspServer::handleInitialize(const JsonValue& params) {
    initialized_ = true;

    // Build capabilities
    JsonValue capabilities = JsonValue::object();

    // Text document sync - incremental updates
    JsonValue textDocSync = JsonValue::object();
    textDocSync.set("openClose", true);
    textDocSync.set("change", 1); // 1 = Full sync, 2 = Incremental
    capabilities.set("textDocumentSync", textDocSync);

    // Completion support
    JsonValue completionProvider = JsonValue::object();
    completionProvider.set("resolveProvider", false);
    JsonValue triggerChars = JsonValue::array();
    triggerChars.push(" ");
    triggerChars.push("@");
    completionProvider.set("triggerCharacters", triggerChars);
    capabilities.set("completionProvider", completionProvider);

    // Hover support
    capabilities.set("hoverProvider", true);

    // Definition support (go-to-definition)
    capabilities.set("definitionProvider", true);

    // Build response
    JsonValue result = JsonValue::object();
    result.set("capabilities", capabilities);

    JsonValue serverInfo = JsonValue::object();
    serverInfo.set("name", "3BX Language Server");
    serverInfo.set("version", "0.1.0");
    result.set("serverInfo", serverInfo);

    return result;
}

void LspServer::handleInitialized(const JsonValue& params) {
    log("Client initialized");
}

void LspServer::handleShutdown() {
    shutdown_ = true;
    log("Shutdown requested");
}

void LspServer::handleExit() {
    std::exit(shutdown_ ? 0 : 1);
}

void LspServer::handleDidOpen(const JsonValue& params) {
    const JsonValue& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].asString();
    std::string text = textDoc["text"].asString();
    int version = textDoc["version"].asInt();

    TextDocument doc;
    doc.uri = uri;
    doc.content = text;
    doc.version = version;
    documents_[uri] = doc;

    log("Document opened: " + uri);
    extractPatternDefinitions(uri, text);

    // Also process imports to get patterns from imported files
    processImports(uri, text);

    publishDiagnostics(uri, text);
}

void LspServer::handleDidChange(const JsonValue& params) {
    const JsonValue& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].asString();
    int version = textDoc["version"].asInt();

    // For full sync, take the complete new content
    const JsonValue& changes = params["contentChanges"];
    if (changes.isArray() && !changes.asArray().empty()) {
        std::string text = changes.asArray()[0]["text"].asString();

        documents_[uri].content = text;
        documents_[uri].version = version;

        log("Document changed: " + uri);
        extractPatternDefinitions(uri, text);

        publishDiagnostics(uri, text);
    }
}

void LspServer::handleDidClose(const JsonValue& params) {
    const JsonValue& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].asString();

    documents_.erase(uri);
    patternDefinitions_.erase(uri);
    log("Document closed: " + uri);

    // Clear diagnostics
    JsonValue diagParams = JsonValue::object();
    diagParams.set("uri", uri);
    diagParams.set("diagnostics", JsonValue::array());
    sendNotification("textDocument/publishDiagnostics", diagParams);
}

JsonValue LspServer::handleCompletion(const JsonValue& params) {
    const JsonValue& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].asString();
    const JsonValue& position = params["position"];
    int line = position["line"].asInt();
    int character = position["character"].asInt();

    JsonValue items = JsonValue::array();

    // Add reserved words
    std::vector<std::string> keywords = {
        "set", "to", "if", "then", "else", "while", "loop",
        "function", "return", "is", "the", "a", "an", "and", "or", "not",
        "pattern", "syntax", "when", "parsed", "triggered", "priority",
        "import", "use", "from", "class", "expression", "members",
        "created", "new", "of", "with", "by", "each", "member",
        "print", "effect", "get", "patterns", "result"
    };

    for (const auto& kw : keywords) {
        JsonValue item = JsonValue::object();
        item.set("label", kw);
        item.set("kind", 14); // Keyword
        item.set("detail", "keyword");
        items.push(item);
    }

    // Add intrinsics
    std::vector<std::pair<std::string, std::string>> intrinsics = {
        {"@intrinsic(\"store\", var, val)", "Store value in variable"},
        {"@intrinsic(\"load\", var)", "Load value from variable"},
        {"@intrinsic(\"add\", a, b)", "Addition"},
        {"@intrinsic(\"sub\", a, b)", "Subtraction"},
        {"@intrinsic(\"mul\", a, b)", "Multiplication"},
        {"@intrinsic(\"div\", a, b)", "Division"},
        {"@intrinsic(\"print\", val)", "Print to console"}
    };

    for (const auto& intr : intrinsics) {
        JsonValue item = JsonValue::object();
        item.set("label", intr.first);
        item.set("kind", 3); // Function
        item.set("detail", intr.second);
        item.set("insertText", intr.first);
        items.push(item);
    }

    // Add patterns from registry
    for (const auto* pattern : registry_->allPatterns()) {
        std::string label;
        for (const auto& elem : pattern->elements) {
            if (!label.empty()) label += " ";
            if (elem.is_param) {
                label += "<" + elem.value + ">";
            } else {
                label += elem.value;
            }
        }

        JsonValue item = JsonValue::object();
        item.set("label", label);
        item.set("kind", 15); // Snippet
        item.set("detail", "pattern");
        items.push(item);
    }

    return items;
}

JsonValue LspServer::handleHover(const JsonValue& params) {
    const JsonValue& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].asString();
    const JsonValue& position = params["position"];
    int line = position["line"].asInt();
    int character = position["character"].asInt();

    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        return JsonValue();
    }

    const std::string& content = it->second.content;

    // Find the word at the cursor position
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string lineStr;
    while (std::getline(stream, lineStr)) {
        lines.push_back(lineStr);
    }

    if (line < 0 || line >= (int)lines.size()) {
        return JsonValue();
    }

    const std::string& currentLine = lines[line];
    if (character < 0 || character >= (int)currentLine.size()) {
        return JsonValue();
    }

    // Find word boundaries
    int start = character;
    int end = character;
    while (start > 0 && (std::isalnum(currentLine[start - 1]) || currentLine[start - 1] == '_' || currentLine[start - 1] == '@')) {
        start--;
    }
    while (end < (int)currentLine.size() && (std::isalnum(currentLine[end]) || currentLine[end] == '_')) {
        end++;
    }

    std::string word = currentLine.substr(start, end - start);

    if (word.empty()) {
        return JsonValue();
    }

    // Check for intrinsics
    if (word == "@intrinsic" || word.find("@") == 0) {
        JsonValue contents = JsonValue::object();
        contents.set("kind", "markdown");
        contents.set("value",
            "**@intrinsic(name, args...)**\n\n"
            "Calls a built-in operation.\n\n"
            "Available intrinsics:\n"
            "- `store(var, val)` - Store value in variable\n"
            "- `load(var)` - Load value from variable\n"
            "- `add(a, b)` - Addition\n"
            "- `sub(a, b)` - Subtraction\n"
            "- `mul(a, b)` - Multiplication\n"
            "- `div(a, b)` - Division\n"
            "- `print(val)` - Print to console"
        );

        JsonValue result = JsonValue::object();
        result.set("contents", contents);
        return result;
    }

    // Check for keywords
    std::unordered_map<std::string, std::string> keywordDocs = {
        {"pattern", "**pattern:**\n\nDefines a new syntax pattern that can be used in code."},
        {"syntax", "**syntax:**\n\nSpecifies the pattern's syntax template. Reserved words become literals, others become parameters."},
        {"when", "**when triggered/parsed:**\n\nDefines behavior when pattern is triggered (runtime) or parsed (compile-time)."},
        {"triggered", "**when triggered:**\n\nRuntime behavior using intrinsics."},
        {"parsed", "**when parsed:**\n\nCompile-time behavior (optional)."},
        {"set", "**set variable to value**\n\nAssigns a value to a variable."},
        {"if", "**if condition then**\n\nConditional statement."},
        {"function", "**function name(params):**\n\nDefines a function."},
        {"import", "**import module.3bx**\n\nImports patterns from another file."},
        {"class", "**class:**\n\nDefines a class with members and patterns."},
    };

    auto docIt = keywordDocs.find(word);
    if (docIt != keywordDocs.end()) {
        JsonValue contents = JsonValue::object();
        contents.set("kind", "markdown");
        contents.set("value", docIt->second);

        JsonValue result = JsonValue::object();
        result.set("contents", contents);
        return result;
    }

    return JsonValue();
}

JsonValue LspServer::handleDefinition(const JsonValue& params) {
    const JsonValue& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].asString();
    const JsonValue& position = params["position"];
    int line = position["line"].asInt();
    int character = position["character"].asInt();

    log("handleDefinition: URI=" + uri + ", line=" + std::to_string(line) + ", char=" + std::to_string(character));

    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        log("Document not found in cache");
        return JsonValue();
    }

    const std::string& content = it->second.content;

    // Get the line at the cursor position
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string lineStr;
    while (std::getline(stream, lineStr)) {
        lines.push_back(lineStr);
    }

    if (line < 0 || line >= (int)lines.size()) {
        log("Line out of range");
        return JsonValue();
    }

    const std::string& currentLine = lines[line];
    log("Current line: \"" + currentLine + "\"");

    // Extract all words from the line with their positions
    struct WordInfo {
        std::string word;
        int startPos;
        int endPos;  // exclusive (position after last char)
    };
    std::vector<WordInfo> lineWords;
    std::string currentWord;
    int wordStart = -1;
    std::string clickedWord;
    int clickedWordIndex = -1;

    for (int charIndex = 0; charIndex <= (int)currentLine.size(); charIndex++) {
        char c = (charIndex < (int)currentLine.size()) ? currentLine[charIndex] : ' ';
        if (std::isalnum(c) || c == '_') {
            if (currentWord.empty()) {
                wordStart = charIndex;
            }
            currentWord += c;
        } else if (!currentWord.empty()) {
            WordInfo info;
            info.word = currentWord;
            info.startPos = wordStart;
            info.endPos = charIndex;  // exclusive
            lineWords.push_back(info);

            // Check if cursor is within this word (inclusive start, exclusive end)
            // LSP character position is 0-based and points to the character under the cursor
            // When clicking on a character, the cursor position is that character's index
            if (character >= wordStart && character < charIndex) {
                clickedWord = currentWord;
                clickedWordIndex = (int)lineWords.size() - 1;
                log("  -> Cursor is within word \"" + currentWord + "\" (pos " +
                    std::to_string(wordStart) + "-" + std::to_string(charIndex) + ")");
            }
            currentWord.clear();
            wordStart = -1;
        }
    }

    if (lineWords.empty()) {
        log("No words found on line");
        return JsonValue();
    }

    log("Line words: ");
    for (size_t idx = 0; idx < lineWords.size(); idx++) {
        log("  [" + std::to_string(idx) + "] \"" + lineWords[idx].word + "\" (pos " +
            std::to_string(lineWords[idx].startPos) + "-" + std::to_string(lineWords[idx].endPos) + ")");
    }

    // If no word was found at cursor position, check if cursor is just past the last character of a word
    if (clickedWord.empty() && !lineWords.empty()) {
        for (size_t idx = 0; idx < lineWords.size(); idx++) {
            // Check if cursor is exactly at the end position of a word
            if (character == lineWords[idx].endPos) {
                clickedWord = lineWords[idx].word;
                clickedWordIndex = (int)idx;
                log("  -> Cursor is at end of word \"" + clickedWord + "\"");
                break;
            }
        }
    }

    log("Clicked word: \"" + clickedWord + "\"");

    if (clickedWord.empty()) {
        log("No word found at cursor position");
    }

    // Convert lineWords to simple strings for pattern matching
    std::vector<std::string> lineWordStrings;
    for (const auto& wi : lineWords) {
        lineWordStrings.push_back(wi.word);
    }

    // Strategy 1: If we have a clicked word, look for patterns where the clicked word
    // is the FIRST literal word in the pattern syntax. This ensures that clicking on
    // "print" in "print testVector" matches "effect print value:" and not "expression left + right:"
    if (!clickedWord.empty()) {
        std::string clickedLower = clickedWord;
        std::transform(clickedLower.begin(), clickedLower.end(), clickedLower.begin(), ::tolower);

        log("Looking for patterns starting with literal \"" + clickedWord + "\"");

        for (const auto& docPair : patternDefinitions_) {
            for (const auto& patDef : docPair.second) {
                // Find the FIRST literal (non-parameter) word in the pattern
                std::string firstLiteral;
                for (const auto& pw : patDef.words) {
                    // Skip parameters (they start with <)
                    if (!pw.empty() && pw[0] == '<') {
                        continue;
                    }
                    // Found first literal
                    firstLiteral = pw;
                    break;
                }

                if (firstLiteral.empty()) {
                    continue;
                }

                std::string firstLiteralLower = firstLiteral;
                std::transform(firstLiteralLower.begin(), firstLiteralLower.end(), firstLiteralLower.begin(), ::tolower);

                if (firstLiteralLower == clickedLower) {
                    log("Found pattern with matching first literal: \"" + patDef.syntax + "\"");

                    // Return the location of this pattern definition
                    JsonValue result = JsonValue::object();
                    result.set("uri", patDef.location.uri);

                    JsonValue range = JsonValue::object();
                    JsonValue start = JsonValue::object();
                    start.set("line", patDef.location.range.start.line);
                    start.set("character", patDef.location.range.start.character);
                    JsonValue end = JsonValue::object();
                    end.set("line", patDef.location.range.end.line);
                    end.set("character", patDef.location.range.end.character);
                    range.set("start", start);
                    range.set("end", end);
                    result.set("range", range);

                    return result;
                }
            }
        }
        log("No pattern found with first literal \"" + clickedWord + "\"");
    }

    // Strategy 2: Try to match the entire line's words against pattern definitions
    log("Strategy 2: Trying full line pattern matching");
    for (const auto& docPair : patternDefinitions_) {
        for (const auto& patDef : docPair.second) {
            log("Checking pattern: \"" + patDef.syntax + "\" with words:");
            for (const auto& pw : patDef.words) {
                log("    pattern word: \"" + pw + "\"");
            }

            // Check if this pattern could match the line
            // A pattern matches if its literal words appear in sequence in lineWords
            bool matches = true;
            size_t lineWordIdx = 0;

            for (const auto& patWord : patDef.words) {
                // Skip parameter placeholders (they start with < and end with >)
                if (!patWord.empty() && patWord[0] == '<') {
                    // This is a parameter - it can match any word
                    if (lineWordIdx < lineWordStrings.size()) {
                        log("    param \"" + patWord + "\" matches \"" + lineWordStrings[lineWordIdx] + "\"");
                        lineWordIdx++;
                    }
                    continue;
                }

                // Find this literal word in remaining lineWords
                bool found = false;
                while (lineWordIdx < lineWordStrings.size()) {
                    // Case-insensitive comparison
                    std::string lw = lineWordStrings[lineWordIdx];
                    std::string pw = patWord;
                    std::transform(lw.begin(), lw.end(), lw.begin(), ::tolower);
                    std::transform(pw.begin(), pw.end(), pw.begin(), ::tolower);

                    if (lw == pw) {
                        log("    literal \"" + patWord + "\" matches \"" + lineWordStrings[lineWordIdx] + "\"");
                        found = true;
                        lineWordIdx++;
                        break;
                    }
                    // Move to next word (might be a parameter value)
                    log("    skipping \"" + lineWordStrings[lineWordIdx] + "\" (looking for \"" + patWord + "\")");
                    lineWordIdx++;
                }

                if (!found) {
                    log("    literal \"" + patWord + "\" NOT found - no match");
                    matches = false;
                    break;
                }
            }

            if (matches && lineWordIdx > 0) {
                log("Found matching pattern: " + patDef.syntax);

                // Return the location
                JsonValue result = JsonValue::object();
                result.set("uri", patDef.location.uri);

                JsonValue range = JsonValue::object();
                JsonValue start = JsonValue::object();
                start.set("line", patDef.location.range.start.line);
                start.set("character", patDef.location.range.start.character);
                JsonValue end = JsonValue::object();
                end.set("line", patDef.location.range.end.line);
                end.set("character", patDef.location.range.end.character);
                range.set("start", start);
                range.set("end", end);
                result.set("range", range);

                return result;
            }
        }
    }

    // Also check patterns from the registry (these are built-in patterns)
    for (const auto* pattern : registry_->allPatterns()) {
        if (pattern->definition && pattern->definition->location.filename.size() > 0) {
            // Build the pattern words
            std::vector<std::string> patWords;
            for (const auto& elem : pattern->elements) {
                if (elem.is_param) {
                    patWords.push_back("<" + elem.value + ">");
                } else {
                    patWords.push_back(elem.value);
                }
            }

            // Check if this pattern matches
            bool matches = true;
            size_t lineWordIdx = 0;

            for (const auto& patWord : patWords) {
                if (!patWord.empty() && patWord[0] == '<') {
                    lineWordIdx++;
                    continue;
                }

                bool found = false;
                while (lineWordIdx < lineWordStrings.size()) {
                    std::string lw = lineWordStrings[lineWordIdx];
                    std::string pw = patWord;
                    std::transform(lw.begin(), lw.end(), lw.begin(), ::tolower);
                    std::transform(pw.begin(), pw.end(), pw.begin(), ::tolower);

                    if (lw == pw) {
                        found = true;
                        lineWordIdx++;
                        break;
                    }
                    lineWordIdx++;
                }

                if (!found) {
                    matches = false;
                    break;
                }
            }

            if (matches && lineWordIdx > 0) {
                log("Found matching registry pattern");

                // Convert file path to URI
                std::string fileUri = pathToUri(pattern->definition->location.filename);

                JsonValue result = JsonValue::object();
                result.set("uri", fileUri);

                JsonValue range = JsonValue::object();
                JsonValue start = JsonValue::object();
                start.set("line", (int)(pattern->definition->location.line - 1));  // LSP is 0-indexed
                start.set("character", (int)(pattern->definition->location.column - 1));
                JsonValue end = JsonValue::object();
                end.set("line", (int)(pattern->definition->location.line - 1));
                end.set("character", (int)(pattern->definition->location.column + 10));  // Approximate end
                range.set("start", start);
                range.set("end", end);
                result.set("range", range);

                return result;
            }
        }
    }

    log("No matching pattern found");
    return JsonValue();
}

void LspServer::extractPatternDefinitions(const std::string& uri, const std::string& content) {
    // Clear existing definitions for this document
    patternDefinitions_[uri].clear();

    log("extractPatternDefinitions for " + uri);

    // Reserved words in 3BX - these are literals in patterns, not parameters
    static const std::unordered_set<std::string> reservedWords = {
        "set", "to", "if", "then", "else", "while", "loop", "function", "return",
        "is", "the", "a", "an", "and", "or", "not", "pattern", "syntax", "when",
        "parsed", "triggered", "priority", "import", "use", "from", "class",
        "expression", "members", "created", "new", "of", "with", "by", "each",
        "member", "print", "effect", "get", "patterns", "result", "condition",
        "section", "equal", "less", "greater", "than", "write", "console",
        "multiply", "divide", "add", "subtract", "next", "power", "two", "s"
    };

    // Parse the content line by line looking for pattern definitions
    // Patterns look like:
    //   effect set var to val:
    //   expression left + right:
    //   section: (followed by indented syntax)
    //   pattern: (followed by indented syntax line)

    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string lineStr;
    while (std::getline(stream, lineStr)) {
        lines.push_back(lineStr);
    }

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& line = lines[i];

        // Check for pattern type keywords at start of line
        std::string trimmedLine = line;
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            trimmedLine = line.substr(firstNonSpace);
        } else {
            continue;  // Empty line
        }

        // Check for pattern definition keywords
        bool isPatternDef = false;
        std::string syntaxPart;

        // Handle different pattern types: effect, expression, condition, section, pattern
        std::vector<std::string> patternKeywords = {"effect ", "expression ", "condition ", "section ", "pattern:"};

        for (const auto& kw : patternKeywords) {
            if (trimmedLine.find(kw) == 0) {
                isPatternDef = true;
                // Extract the syntax part (everything after the keyword until the colon)
                syntaxPart = trimmedLine.substr(kw.size());
                // Remove trailing colon if present
                if (!syntaxPart.empty() && syntaxPart.back() == ':') {
                    syntaxPart.pop_back();
                }
                break;
            }
        }

        // Handle old-style pattern definitions with "syntax:" on next line
        if (trimmedLine == "pattern:" && i + 1 < lines.size()) {
            const std::string& nextLine = lines[i + 1];
            size_t syntaxPos = nextLine.find("syntax:");
            if (syntaxPos != std::string::npos) {
                isPatternDef = true;
                syntaxPart = nextLine.substr(syntaxPos + 7);
                // Trim leading spaces
                size_t firstNonSpace2 = syntaxPart.find_first_not_of(" \t");
                if (firstNonSpace2 != std::string::npos) {
                    syntaxPart = syntaxPart.substr(firstNonSpace2);
                }
            }
        }

        if (isPatternDef && !syntaxPart.empty()) {
            // Trim the syntax part
            while (!syntaxPart.empty() && std::isspace(syntaxPart.back())) {
                syntaxPart.pop_back();
            }
            while (!syntaxPart.empty() && std::isspace(syntaxPart.front())) {
                syntaxPart = syntaxPart.substr(1);
            }

            if (syntaxPart.empty()) {
                continue;
            }

            PatternDefLocation patDef;
            patDef.syntax = syntaxPart;
            patDef.location.uri = uri;
            patDef.location.range.start.line = (int)i;
            patDef.location.range.start.character = (int)firstNonSpace;
            patDef.location.range.end.line = (int)i;
            patDef.location.range.end.character = (int)line.size();

            // Extract words from the syntax, marking reserved words as literals
            // and non-reserved words as parameters
            // IMPORTANT: For single-word patterns like "test", the word itself IS the pattern name
            // and should be treated as a literal, not a parameter
            std::string word;
            std::vector<std::string> allWords;  // Collect all words first
            for (char c : syntaxPart) {
                if (std::isalnum(c) || c == '_') {
                    word += c;
                } else if (!word.empty()) {
                    allWords.push_back(word);
                    word.clear();
                }
            }
            if (!word.empty()) {
                allWords.push_back(word);
            }

            // Now classify each word
            for (size_t wi = 0; wi < allWords.size(); wi++) {
                const std::string& w = allWords[wi];
                std::string lowerWord = w;
                std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);

                // A word is a literal if:
                // 1. It's a reserved word, OR
                // 2. It's the ONLY word in the pattern (single-word patterns are always literals)
                bool isLiteral = reservedWords.count(lowerWord) || allWords.size() == 1;

                if (isLiteral) {
                    patDef.words.push_back(w);  // Literal
                } else {
                    patDef.words.push_back("<" + w + ">");  // Parameter
                }
            }

            patternDefinitions_[uri].push_back(patDef);
            log("Found pattern: \"" + patDef.syntax + "\"");
        }
    }

    log("Extracted " + std::to_string(patternDefinitions_[uri].size()) + " patterns from " + uri);
}

void LspServer::processImports(const std::string& uri, const std::string& content) {
    log("=== processImports for " + uri + " ===");

    // Get the directory of the source file
    std::string filePath = uriToPath(uri);
    std::string sourceDir;
    size_t lastSlash = filePath.rfind('/');
    if (lastSlash != std::string::npos) {
        sourceDir = filePath.substr(0, lastSlash);
    } else {
        sourceDir = ".";
    }

    // Parse the content line by line looking for import statements
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string lineStr;
    while (std::getline(stream, lineStr)) {
        lines.push_back(lineStr);
    }

    std::set<std::string> processedImports;

    for (const auto& line : lines) {
        // Trim the line
        std::string trimmedLine = line;
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            trimmedLine = line.substr(firstNonSpace);
        } else {
            continue;
        }

        // Check for "import " at the start
        if (trimmedLine.find("import ") == 0) {
            std::string importPath = trimmedLine.substr(7);
            // Trim trailing whitespace
            while (!importPath.empty() && std::isspace(importPath.back())) {
                importPath.pop_back();
            }

            if (importPath.empty()) {
                continue;
            }

            log("Found import: \"" + importPath + "\"");

            // Resolve the import path
            std::string resolvedPath = resolveImportPath(importPath, sourceDir);
            if (resolvedPath.empty()) {
                log("  Could not resolve import path");
                continue;
            }

            // Skip if already processed
            if (processedImports.count(resolvedPath)) {
                continue;
            }
            processedImports.insert(resolvedPath);

            log("  Resolved to: " + resolvedPath);

            // Read and process the imported file
            std::ifstream file(resolvedPath);
            if (!file) {
                log("  Could not open file");
                continue;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string importedContent = buffer.str();

            std::string importedUri = pathToUri(resolvedPath);
            log("  Extracting patterns from " + importedUri);

            // Extract pattern definitions from the imported file
            extractPatternDefinitions(importedUri, importedContent);
        }
    }
}

std::string LspServer::resolveImportPath(const std::string& importPath, const std::string& sourceDir) {
    // Try the path as-is relative to source directory
    std::string fullPath = sourceDir + "/" + importPath;
    std::ifstream test1(fullPath);
    if (test1) {
        return fullPath;
    }

    // Try lib directory relative to source
    fullPath = sourceDir + "/lib/" + importPath;
    std::ifstream test2(fullPath);
    if (test2) {
        return fullPath;
    }

    // Search up the directory tree for lib folder (up to 5 levels)
    std::string searchDir = sourceDir;
    for (int level = 0; level < 5; level++) {
        fullPath = searchDir + "/lib/" + importPath;
        std::ifstream test3(fullPath);
        if (test3) {
            return fullPath;
        }

        // Go up one directory
        size_t lastSlash = searchDir.rfind('/');
        if (lastSlash == std::string::npos || lastSlash == 0) {
            break;
        }
        searchDir = searchDir.substr(0, lastSlash);
    }

    return "";
}

void LspServer::publishDiagnostics(const std::string& uri, const std::string& content) {
    std::string path = uriToPath(uri);
    std::vector<LspDiagnostic> diags = getDiagnostics(content, path);

    JsonValue diagnostics = JsonValue::array();
    for (const auto& diag : diags) {
        JsonValue d = JsonValue::object();

        JsonValue range = JsonValue::object();
        JsonValue start = JsonValue::object();
        start.set("line", diag.range.start.line);
        start.set("character", diag.range.start.character);
        JsonValue end = JsonValue::object();
        end.set("line", diag.range.end.line);
        end.set("character", diag.range.end.character);
        range.set("start", start);
        range.set("end", end);

        d.set("range", range);
        d.set("severity", diag.severity);
        d.set("source", diag.source);
        d.set("message", diag.message);

        diagnostics.push(d);
    }

    JsonValue params = JsonValue::object();
    params.set("uri", uri);
    params.set("diagnostics", diagnostics);

    sendNotification("textDocument/publishDiagnostics", params);
}

std::vector<LspDiagnostic> LspServer::getDiagnostics(const std::string& content, const std::string& filename) {
    std::vector<LspDiagnostic> diagnostics;

    try {
        // Use the lexer to tokenize and find errors
        Lexer lexer(content, filename);
        auto tokens = lexer.tokenize();

        // Check for lexer errors
        for (const auto& token : tokens) {
            if (token.type == TokenType::ERROR) {
                LspDiagnostic diag;
                diag.range.start.line = (int)token.location.line - 1;
                diag.range.start.character = (int)token.location.column - 1;
                diag.range.end.line = (int)token.location.line - 1;
                diag.range.end.character = (int)token.location.column + (int)token.lexeme.size() - 1;
                diag.severity = 1; // Error
                diag.message = "Unexpected token: " + token.lexeme;
                diagnostics.push_back(diag);
            }
        }

        // Try to parse and catch syntax errors
        Lexer parserLexer(content, filename);
        Parser parser(parserLexer);

        try {
            auto program = parser.parse();
        } catch (const std::exception& e) {
            // Parse error - try to extract location from message
            std::string msg = e.what();
            LspDiagnostic diag;
            diag.range.start.line = 0;
            diag.range.start.character = 0;
            diag.range.end.line = 0;
            diag.range.end.character = 0;
            diag.severity = 1;
            diag.message = msg;

            // Try to parse line/column from error message
            size_t linePos = msg.find("line ");
            if (linePos != std::string::npos) {
                size_t numStart = linePos + 5;
                size_t numEnd = numStart;
                while (numEnd < msg.size() && std::isdigit(msg[numEnd])) numEnd++;
                if (numEnd > numStart) {
                    diag.range.start.line = std::stoi(msg.substr(numStart, numEnd - numStart)) - 1;
                    diag.range.end.line = diag.range.start.line;
                }
            }

            diagnostics.push_back(diag);
        }
    } catch (const std::exception& e) {
        // General error during analysis
        LspDiagnostic diag;
        diag.range.start.line = 0;
        diag.range.start.character = 0;
        diag.range.end.line = 0;
        diag.range.end.character = 0;
        diag.severity = 1;
        diag.message = std::string("Analysis error: ") + e.what();
        diagnostics.push_back(diag);
    }

    return diagnostics;
}

void LspServer::sendResponse(const JsonValue& id, const JsonValue& result) {
    JsonValue response = JsonValue::object();
    response.set("jsonrpc", "2.0");
    response.set("id", id);
    response.set("result", result);
    writeMessage(response.serialize());
}

void LspServer::sendError(const JsonValue& id, int code, const std::string& message) {
    JsonValue error = JsonValue::object();
    error.set("code", code);
    error.set("message", message);

    JsonValue response = JsonValue::object();
    response.set("jsonrpc", "2.0");
    response.set("id", id);
    response.set("error", error);
    writeMessage(response.serialize());
}

void LspServer::sendNotification(const std::string& method, const JsonValue& params) {
    JsonValue notification = JsonValue::object();
    notification.set("jsonrpc", "2.0");
    notification.set("method", method);
    notification.set("params", params);
    writeMessage(notification.serialize());
}

void LspServer::log(const std::string& message) {
    if (debug_) {
        std::cerr << "[3BX-LSP] " << message << std::endl;
    }
}

std::string LspServer::uriToPath(const std::string& uri) {
    // Simple file:// URI to path conversion
    if (uri.find("file://") == 0) {
        std::string path = uri.substr(7);
        // URL decode (simplified - just handle %20 for spaces)
        std::string decoded;
        for (size_t i = 0; i < path.size(); i++) {
            if (path[i] == '%' && i + 2 < path.size()) {
                int val = std::stoi(path.substr(i + 1, 2), nullptr, 16);
                decoded += (char)val;
                i += 2;
            } else {
                decoded += path[i];
            }
        }
        return decoded;
    }
    return uri;
}

std::string LspServer::pathToUri(const std::string& path) {
    return "file://" + path;
}

} // namespace tbx
