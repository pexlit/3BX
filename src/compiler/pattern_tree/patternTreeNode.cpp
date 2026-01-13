#include "patternTreeNode.h"

void PatternTreeNode::addPatternPart(std::vector<PatternElement> &elements, Section *matchingSection, size_t index)
{
	PatternTreeNode *currentNode = this;
	for (; index < elements.size(); index++)
	{
		auto currentElement = elements[index];
		if (!currentNode->literalChildren.count(currentElement.text))
		{
			currentNode->literalChildren[currentElement.text] = new PatternTreeNode(currentElement.type, currentElement.text);
		}
		currentNode = currentNode->literalChildren[currentElement.text];
	}
	currentNode->matchingSection = matchingSection;
}

PatternTreeNode *PatternTreeNode::match(const std::vector<PatternElement> &elements)
{
	PatternTreeNode *currentNode = this;
	for (const PatternElement &element : elements)
	{
		if (currentNode->literalChildren.count(element.text))
		{
			currentNode = currentNode->literalChildren[element.text];
		}
		else
		{
			if (element.type == PatternElement::Type::VariableLike)
			{
				// maybe it's a variable?

			}
			return nullptr;
		}
	}
	if (currentNode->matchingSection)
	{
		// a pattern ends here
		return currentNode;
	}
	return nullptr;
}
