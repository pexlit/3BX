#include <vector>
#include "codeLine.h"
#include "diagnostic.h"
struct ParseContext
{
	std::vector<CodeLine> codeLines;
	std::vector<Diagnostic> diagnostics;
	Section *mainSection{};
};