#include "sectionType.h"
#include "codeLine.h"
#include <vector>
struct Section
{
	SectionType type;
	Section(SectionType type):type(type) {}
	std::vector<CodeLine*> codeLines;
};