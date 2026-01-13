#pragma once
#include "sectionType.h"
#include "patternTreeNode.h"
#include "codeLine.h"
struct ParseContext;
// traversing the tree will also output a tree of possibilities
//  (which way should we search first? should we substitute a literal as variable or not?)
struct MatchProgress
{
	MatchProgress(ParseContext &context, CodeLine *patternLine);
	MatchProgress *parent{};
	ParseContext &context;
	// the pattern type we're currently matching for
	SectionType type;
	// the root node of the current node
	PatternTreeNode *rootNode;
	// the node this step is at, currently.
	PatternTreeNode *currentNode;
	// the tree node which matched successfully
	PatternTreeNode *result;
	CodeLine *patternLine;

	// the nodes this progress passed already
	std::vector<PatternTreeNode *> nodesPassed{};

	int sourceElementIndex{};
	int sourceCharIndex{};
	// returns a vector containing alternative steps we could take through the pattern tree, ordered from least important ([0]) to most important ([length() - 1])
	std::vector<MatchProgress> step();
	//wether this progress can start a submatch
	bool canSubstitute() const;
	//wether this progress can be a submatch
	bool canBeSubstitute() const;
};