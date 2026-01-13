#include "matchProgress.h"
#include "parseContext.h"
MatchProgress::MatchProgress(ParseContext &context, CodeLine *patternLine) : context(context), patternLine(patternLine)
{
}

std::vector<MatchProgress> MatchProgress::step()
{
	std::vector<MatchProgress> nextMatches = std::vector<MatchProgress>();
	PatternElement elementToCompare = patternLine->patternElements[sourceElementIndex];

	// less priority: arguments
	if (currentNode->argumentChild)
	{
		// submatch and use the result as argument for the parent progress
		auto stepDown = [&nextMatches, this](MatchProgress &parentProgress)
		{
			if (parentProgress.currentNode->argumentChild)
			{
				MatchProgress stepDown = parentProgress;
				stepDown.currentNode = context.patternTrees[(int)SectionType::Expression];
				stepDown.parent = new MatchProgress(parentProgress);
				stepDown.nodesPassed = {};
				stepDown.rootNode = stepDown.currentNode;
				nextMatches.push_back(stepDown);
			}
		};
		if (canSubstitute())
		{
			stepDown(*this);
		}
		if (parent)
		{
			// this might be a submatch of a higher level match.
			//step
		}
		if (canBeSubstitute())
		{
			// this might be the first submatch of a higher level match.
			MatchProgress stepUp = *this;
			// the old parent progress becomes 'grandparent'
			stepUp.parent = new MatchProgress(*parent);
			stepUp.rootNode = rootNode;
			stepUp.currentNode = rootNode;

			stepUp.type = SectionType::Expression;
			stepDown(stepUp);
		}

		// use an element as argument
		if (elementToCompare.type != PatternElement::Type::Other)
		{
			// variable or potential variable
			MatchProgress substituteStep = *this;
			// we continue on the branch that takes an argument now

			substituteStep.currentNode = currentNode->argumentChild;
			substituteStep.nodesPassed.push_back(substituteStep.currentNode);
			substituteStep.sourceElementIndex++;
			nextMatches.push_back(substituteStep);
		}
	}
	// most priority: text match
	if (elementToCompare.type != PatternElement::Type::Variable && currentNode->literalChildren.count(elementToCompare.text))
	{
		MatchProgress elemStep = *this;
		elemStep.currentNode = currentNode->literalChildren[elementToCompare.text];
		elemStep.nodesPassed.push_back(elemStep.currentNode);
		elemStep.sourceElementIndex++;
		nextMatches.push_back(elemStep);
	}
	return nextMatches;
}

bool MatchProgress::canSubstitute() const
{
	// prevent infinite recursion
	return type != SectionType::Expression || currentNode != rootNode;
}

bool MatchProgress::canBeSubstitute() const
{
	return type == SectionType::Expression;
}
