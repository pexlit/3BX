#pragma once
#include "sectionType.h"
#include "codeLine.h"
#include <vector>
#include "patternDefinition.h"
struct ParseContext;
struct Section
{
	inline Section(SectionType type, Section *parent = {}) : type(type), parent(parent)
	{
		if (parent)
			parent->children.push_back(this);
	}
	SectionType type;
	Section *parent{};
	std::vector<PatternDefinition*> patternDefinitions;
	std::vector<CodeLine*> patternReferences;
	std::vector<CodeLine *> codeLines;
	std::vector<Section *> children;
	// the start and end index of this section in compiled lines.
	int startLineIndex, endLineIndex;
	// this section is resolved it's pattern definitions are resolved.
	bool resolved = false;
	void updateResolution();
	void processPatterns(ParseContext& context);
	virtual bool processLine(ParseContext& context, CodeLine *line);
	virtual Section *createSection(ParseContext &context, CodeLine *line);
};