#include "section.h"
#include "effectSection.h"
#include "parseContext.h"
#include "patternTreeNode.h"

void Section::updateResolution()
{
	// check if all child code lines are resolved
	for (Section *section : children)
	{
		if (!section->resolved)
		{
			section->updateResolution();
			if (!section->resolved)
				// didn't help
				return;
		}
	}
	for (CodeLine *line : codeLines)
	{
		if (!line->resolved)
		{
			return;
		}
	}
	// all children resolved
	resolved = true;
}

void Section::processPatterns(ParseContext &context)
{
	bool newResolved = true;
	for (Section *section : children)
	{
		if (!section->resolved)
		{
			section->processPatterns(context);
			if (!section->resolved)
			{
				newResolved = false;
			}
		}
	}
	for (CodeLine *line : patternReferences)
	{
		if (!line->resolved)
		{
			// try resolving the line
			if (context.patternTrees[(int)SectionType::Effect]->match(line->patternElements))
			{
				line->resolved = true;
			}
			else
			{
				newResolved = false;
			}
		}
	}
	if (newResolved)
	{
		//now that we know all child code lines, we can start deducing variables
	}
	resolved = newResolved;
}

bool Section::processLine(ParseContext &, CodeLine *line)
{
	// treat as normal code
	patternReferences.push_back(line);
	return true;
}

Section *Section::createSection(ParseContext &, CodeLine *line)
{
	// determine the section type
	std::size_t spaceIndex = line->patternText.find(' ');
	Section *newSection{};
	if (spaceIndex != std::string::npos)
	{
		std::string sectionTypeString = (std::string)line->patternText.substr(0, spaceIndex);
		if (sectionTypeString == "effect")
		{
			newSection = new EffectSection(this);
		}
		if (newSection)
		{
			// check if there's a pattern right after the name (f.e. "effect set val to var" <- right after "effect ")
			std::string_view sectionPatternString = line->patternText.substr(spaceIndex + 1);
			if (sectionPatternString.length())
			{
				newSection->patternDefinitions.push_back(new PatternDefinition(Range(line, sectionPatternString)));
			}
		}
	}
	if (!newSection)
	{
		// custom section
		newSection = new Section(SectionType::Custom, this);
	}
	return newSection;
}
