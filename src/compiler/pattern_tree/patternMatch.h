#pragma once
#include "patternTreeNode.h"
#include "range.h"
struct PatternMatch
{
	PatternTreeNode* matchedEndNode;
	Range range;
};