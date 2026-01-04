#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include "compiler/patternResolver.hpp"

namespace tbx {

enum class InferredType {
    Unknown,
    Void,
    I1,
    I64,
    F64,
    String
};

std::string typeToString(InferredType type);

struct TypedValue {
    InferredType type = InferredType::Unknown;
    std::variant<std::monostate, int64_t, double, std::string> value;
    std::string variableName;
    bool isLiteral = false;

    static TypedValue fromInt(int64_t val);
    static TypedValue fromDouble(double val);
    static TypedValue fromString(const std::string& val);
    static TypedValue fromVariable(const std::string& name, InferredType type);

    void print(int indent = 0) const;
};

struct TypedParameter {
    std::string name;
    InferredType type = InferredType::Unknown;

    void print(int indent = 0) const;
};

struct TypedPattern {
    ResolvedPattern* pattern = nullptr;
    std::map<std::string, InferredType> parameterTypes;
    InferredType returnType = InferredType::Unknown;
    std::vector<std::string> bodyIntrinsics;

    void print(int indent = 0) const;
};

struct TypedCall {
    PatternMatch* match = nullptr;
    std::map<std::string, TypedValue> typedArguments;
    InferredType resultType = InferredType::Unknown;

    void print(int indent = 0) const;
};

struct IntrinsicInfo {
    std::string name;
    std::vector<std::string> arguments;
    bool hasReturn = false;

    InferredType getReturnType(const std::map<std::string, InferredType>& argTypes) const;
    InferredType getArgumentType(size_t index, const std::map<std::string, InferredType>& knownTypes) const;
};

/**
 * TypeInference - Step 4 of the 3BX compiler pipeline
 *
 * Infers types for pattern parameters and return values based on intrinsic usage.
 */
class TypeInference {
public:
    TypeInference();

    /**
     * Run type inference on resolved patterns
     * @return true if all types were successfully inferred (no errors)
     */
    bool infer(const SectionPatternResolver& resolver);

    /**
     * Get any diagnostics (errors/warnings) found during inference
     */
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    /**
     * Get typed representation of a pattern
     */
    TypedPattern* getTypedPattern(ResolvedPattern* pattern) const {
        auto it = patternToTyped_.find(pattern);
        return it != patternToTyped_.end() ? it->second : nullptr;
    }

    /**
     * Get all typed patterns
     */
    const std::vector<std::unique_ptr<TypedPattern>>& typedPatterns() const {
        return typedPatterns_;
    }

    /**
     * Print results for debugging
     */
    void printResults() const;

private:
    std::unique_ptr<TypedPattern> inferPatternTypes(ResolvedPattern* pattern);
    std::unique_ptr<TypedCall> inferCallTypes(PatternMatch* match);

    std::vector<IntrinsicInfo> parseIntrinsics(const std::string& text);
    IntrinsicInfo parseSingleIntrinsic(const std::string& text, size_t startPos);

    InferredType inferValueType(const ResolvedValue& value);
    TypedValue resolvedToTyped(const ResolvedValue& value, const std::string& varName);

    bool isCompatible(InferredType expected, InferredType actual) const;
    InferredType unifyTypes(InferredType t1, InferredType t2) const;

    std::vector<std::unique_ptr<TypedPattern>> typedPatterns_;
    std::vector<std::unique_ptr<TypedCall>> typedCalls_;
    std::map<ResolvedPattern*, TypedPattern*> patternToTyped_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace tbx
