#include <string>
#include "parseContext.h"
bool compile(const std::string& path, ParseContext& context);
bool importSourceFile(const std::string &path, ParseContext &context);
bool analyzeSections(ParseContext &context);
bool resolvePatterns(ParseContext &context);