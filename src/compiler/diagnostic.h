#include <string>
#include "sourceFile.h"
struct Diagnostic{
	enum class Level
	{
		Info,
		Warning,
		Error
	};
	Level level;
	std::string message;
	SourceFile* sourceFile;
	Diagnostic(Level level, const std::string& message, SourceFile* sourceFile)
		:level(level), message(message), sourceFile(sourceFile) {}
};