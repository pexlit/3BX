#include "effectSection.h"
#include "parseContext.h"
#include "diagnostic.h"

bool EffectSection::processLine(ParseContext& context, CodeLine *line)
{
	context.diagnostics.push_back(Diagnostic(Diagnostic::Level::Error, "Code without execute: section", Range(line, line->patternText)));
	return false;
}

Section *EffectSection::createSection(ParseContext& context, CodeLine *line)
{
	if (line->patternText == "execute")
	{
		return new Section(SectionType::Custom, this);
	}
	else
	{
		context.diagnostics.push_back(Diagnostic(Diagnostic::Level::Error, "Unknown section:" + (std::string)line->patternText, Range(line, line->patternText)));
		return nullptr;
	}
}
