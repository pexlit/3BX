#pragma once

#include <string>
#include <iostream>

namespace tbx {

enum class DiagnosticSeverity {
    Error,
    Warning,
    Information,
    Hint
};

struct Diagnostic {
    std::string message;
    std::string filePath;
    int line;
    int column;      // 0-based start column
    int endLine;    // 1-based end line
    int endColumn;  // 0-based end column
    DiagnosticSeverity severity;

    Diagnostic(std::string msg, std::string file = "", int l = 0, int c = 0, DiagnosticSeverity sev = DiagnosticSeverity::Error)
        : message(std::move(msg)), filePath(std::move(file)), line(l), column(c), endLine(l), endColumn(c + 1), severity(sev) {}

    Diagnostic(std::string msg, std::string file, int l, int c, int el, int ec, DiagnosticSeverity sev = DiagnosticSeverity::Error)
        : message(std::move(msg)), filePath(std::move(file)), line(l), column(c), endLine(el), endColumn(ec), severity(sev) {}

    std::string toString() const {
        std::string location;
        if (!filePath.empty()) {
            location = filePath + ":" + std::to_string(line);
        } else if (line > 0) {
            location = "line " + std::to_string(line);
        }
        
        std::string severityString;
        switch (severity) {
            case DiagnosticSeverity::Error: severityString = "Error"; break;
            case DiagnosticSeverity::Warning: severityString = "Warning"; break;
            case DiagnosticSeverity::Information: severityString = "Info"; break;
            case DiagnosticSeverity::Hint: severityString = "Hint"; break;
        }

        if (!location.empty()) {
            return severityString + " at " + location + ": " + message;
        } else {
            return severityString + ": " + message;
        }
    }
};

} // namespace tbx
