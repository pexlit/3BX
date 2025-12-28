#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantic/semantic.hpp"
#include "codegen/codegen.hpp"
#include "lsp/lspServer.hpp"
#include "dap/dapServer.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <filesystem>

// LLVM JIT includes
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Error.h>

namespace fs = std::filesystem;

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " <source_file.3bx>\n";
    std::cerr << "       " << program << " --emit-ir <source_file.3bx>\n";
    std::cerr << "       " << program << " --lsp [--debug]\n";
    std::cerr << "       " << program << " --dap [--debug]\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --emit-ir    Output LLVM IR instead of compiling\n";
    std::cerr << "  --lsp        Start Language Server Protocol mode\n";
    std::cerr << "  --dap        Start Debug Adapter Protocol mode\n";
    std::cerr << "  --debug      Enable debug logging (with --lsp or --dap)\n";
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Get the directory where the compiler executable is located
std::string getExecutableDir() {
    return fs::canonical("/proc/self/exe").parent_path().string();
}

// Resolve import path relative to source file or lib directory
std::string resolveImport(const std::string& importPath, const std::string& sourceFile) {
    fs::path sourceDir = fs::path(sourceFile).parent_path();
    if (sourceDir.empty()) sourceDir = ".";

    // Try relative to source file first
    fs::path relativePath = sourceDir / importPath;
    if (fs::exists(relativePath)) {
        return relativePath.string();
    }

    // Try lib directory relative to source
    fs::path libPath = sourceDir / "lib" / importPath;
    if (fs::exists(libPath)) {
        return libPath.string();
    }

    // Search up the directory tree for lib folder (up to 5 levels)
    fs::path searchDir = sourceDir;
    for (int i = 0; i < 5; i++) {
        libPath = searchDir / "lib" / importPath;
        if (fs::exists(libPath)) {
            return fs::canonical(libPath).string();
        }
        fs::path parent = searchDir.parent_path();
        if (parent == searchDir) break; // Reached root
        searchDir = parent;
    }

    // Try lib directory relative to executable (for installed compiler)
    std::string exeDir = getExecutableDir();
    libPath = fs::path(exeDir) / ".." / "lib" / importPath;
    if (fs::exists(libPath)) {
        return fs::canonical(libPath).string();
    }

    // Try lib directory next to executable
    libPath = fs::path(exeDir) / "lib" / importPath;
    if (fs::exists(libPath)) {
        return libPath.string();
    }

    // Return original path, let it fail later with proper error
    return importPath;
}

// Extract import paths from a file using lightweight lexing
std::vector<std::string> extractImports(const std::string& source, const std::string& filePath) {
    std::vector<std::string> imports;
    tbx::Lexer lexer(source, filePath);

    while (true) {
        auto token = lexer.nextToken();
        if (token.type == tbx::TokenType::END_OF_FILE) break;

        if (token.lexeme == "import") {
            // Check if next token is 'function' (skip import function declarations)
            auto next = lexer.peek();
            if (next.lexeme == "function") {
                continue;
            }

            // Collect import path
            std::string path;
            while (true) {
                auto pathToken = lexer.nextToken();
                if (pathToken.type == tbx::TokenType::NEWLINE ||
                    pathToken.type == tbx::TokenType::END_OF_FILE) {
                    break;
                }
                path += pathToken.lexeme;
            }
            if (!path.empty()) {
                imports.push_back(path);
            }
        }
        else if (token.lexeme == "use") {
            // use <name> from <path>
            // Skip to 'from'
            while (true) {
                auto t = lexer.nextToken();
                if (t.lexeme == "from") {
                    break;
                }
                if (t.type == tbx::TokenType::NEWLINE ||
                    t.type == tbx::TokenType::END_OF_FILE) {
                    break;
                }
            }
            // Collect path after 'from'
            std::string path;
            while (true) {
                auto pathToken = lexer.nextToken();
                if (pathToken.type == tbx::TokenType::NEWLINE ||
                    pathToken.type == tbx::TokenType::END_OF_FILE) {
                    break;
                }
                path += pathToken.lexeme;
            }
            if (!path.empty()) {
                imports.push_back(path);
            }
        }
    }
    return imports;
}

// Phase 1: Collect all files and their imports (depth-first)
void collectFiles(
    const std::string& filePath,
    std::set<std::string>& visitedFiles,
    std::vector<std::string>& orderedFiles
) {
    fs::path canonical = fs::weakly_canonical(filePath);
    std::string canonicalPath = canonical.string();

    if (visitedFiles.count(canonicalPath)) {
        return;
    }
    visitedFiles.insert(canonicalPath);

    // Read and extract imports
    std::string source = readFile(filePath);
    auto imports = extractImports(source, filePath);

    // Process imports first (depth-first)
    for (const auto& importPath : imports) {
        std::string resolved = resolveImport(importPath, filePath);
        collectFiles(resolved, visitedFiles, orderedFiles);
    }

    // Add this file after its imports
    orderedFiles.push_back(canonicalPath);
}

// Parse a file and collect its imports recursively
// Uses shared registry to accumulate patterns across files
#include "pattern/pattern_resolution.hpp"

// ... (keep includes consistent)

void parseWithImports(
    const std::string& filePath,
    std::set<std::string>& parsedFiles,
    std::vector<tbx::StmtPtr>& allStatements,
    tbx::PatternRegistry& sharedRegistry,
    const std::string& originalSource
) {
    // ... Phase 1 ...
    std::set<std::string> visitedFiles;
    std::vector<std::string> orderedFiles;

    // Auto-load prelude.3bx
    std::string preludePath = resolveImport("prelude.3bx", filePath);
    if (fs::exists(preludePath)) {
        collectFiles(preludePath, visitedFiles, orderedFiles);
    } else {
        std::cerr << "Warning: prelude.3bx not found in library paths.\n";
    }

    collectFiles(filePath, visitedFiles, orderedFiles);

    // Phase 2: Parse files in order
    for (const auto& file : orderedFiles) {
        if (parsedFiles.count(file)) {
            continue;
        }
        parsedFiles.insert(file);

        // Read and parse the file
        std::string source = readFile(file);
        tbx::Lexer lexer(source, file);
        tbx::Parser parser(lexer);

        parser.setSharedRegistry(&sharedRegistry);

        auto program = parser.parse();

        // Add non-import statements
        for (auto& stmt : program->statements) {
            if (!dynamic_cast<tbx::ImportStmt*>(stmt.get()) &&
                !dynamic_cast<tbx::UseStmt*>(stmt.get())) {
                allStatements.push_back(std::move(stmt));
            }
        }
        
        // Resolve newly added patterns so they are available for subsequent files
        // This calculates variables, specificity, and priorities
        tbx::PatternResolver resolver(sharedRegistry);
        resolver.resolveAll();
    }
}

// Run the compiled module using LLVM ORC JIT
int runJit(std::unique_ptr<llvm::LLVMContext> context, std::unique_ptr<llvm::Module> module) {
    // Initialize native target
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Create JIT
    auto jitExpected = llvm::orc::LLJITBuilder().create();
    if (!jitExpected) {
        std::cerr << "Error creating JIT: " << llvm::toString(jitExpected.takeError()) << "\n";
        return 1;
    }
    auto jit = std::move(*jitExpected);

    // Add the module to JIT
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));
    if (auto err = jit->addIRModule(std::move(tsm))) {
        std::cerr << "Error adding module to JIT: " << llvm::toString(std::move(err)) << "\n";
        return 1;
    }

    // Look up the main function
    auto mainSymbol = jit->lookup("main");
    if (!mainSymbol) {
        std::cerr << "Error looking up main: " << llvm::toString(mainSymbol.takeError()) << "\n";
        return 1;
    }

    // Get the function pointer and call it
    auto* mainFn = mainSymbol->toPtr<int()>();
    return mainFn();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    bool emitIr = false;
    bool lspMode = false;
    bool dapMode = false;
    bool debugMode = false;
    std::string sourceFile;

    for (int argIndex = 1; argIndex < argc; argIndex++) {
        std::string arg = argv[argIndex];
        if (arg == "--emit-ir") {
            emitIr = true;
        } else if (arg == "--lsp") {
            lspMode = true;
        } else if (arg == "--dap") {
            dapMode = true;
        } else if (arg == "--debug") {
            debugMode = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            sourceFile = arg;
        }
    }

    // Handle LSP mode
    if (lspMode) {
        tbx::LspServer server;
        server.setDebug(debugMode);
        server.run();
        return 0;
    }

    // Handle DAP mode
    if (dapMode) {
        tbx::DapServer server;
        server.setDebug(debugMode);
        server.run();
        return 0;
    }

    if (sourceFile.empty()) {
        std::cerr << "Error: No source file specified\n";
        return 1;
    }

    try {
        // Create shared pattern registry for all files
        tbx::PatternRegistry sharedRegistry;

        // Parse main file and all imports recursively
        std::set<std::string> parsedFiles;
        std::vector<tbx::StmtPtr> allStatements;
        parseWithImports(sourceFile, parsedFiles, allStatements, sharedRegistry, sourceFile);

        // Create combined program
        auto program = std::make_unique<tbx::Program>();
        program->statements = std::move(allStatements);

        // Semantic analysis
        tbx::SemanticAnalyzer analyzer;
        if (!analyzer.analyze(*program)) {
            for (const auto& err : analyzer.errors()) {
                std::cerr << "Error: " << err << "\n";
            }
            return 1;
        }

        // Code generation
        tbx::CodeGenerator codegen(sourceFile);
        codegen.setPatternRegistry(&sharedRegistry);
        if (!codegen.generate(*program)) {
            std::cerr << "Error: Code generation failed\n";
            return 1;
        }

        if (emitIr) {
            codegen.getModule()->print(llvm::outs(), nullptr);
            return 0;
        }

        // Run the compiled code using JIT
        auto context = codegen.takeContext();
        auto module = codegen.takeModule();
        return runJit(std::move(context), std::move(module));

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
