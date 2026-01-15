#pragma once
#include "range.h"
#include <vector>
#include "pattern/pattern_tree/patternElement.h"
#include <ranges>
#include <functional>
#include <list>

//the BEL charachter is supposed to ring a bell in terminals. since this is code, there's no reason for the user to need this charachter.
constexpr char argumentChar = '\a';
/**
 * when modifying a line, you'll be able to find where a part of the line was first.
 * for example, if the line was 'i like bread' first and it becomes 'i $ bread',
 * then a keyframe will be added at the space in the pattern behind the '$' to point to the space in the pattern behind the 'like'
 */
struct TransformedPattern
{
	struct KeyFrame
	{
		size_t patternPos;
		size_t linePos;
	};
	typedef std::function<size_t(const KeyFrame&)> keyFrameTransformer;
	std::string text;
	std::vector<PatternElement> elements;
	std::list<KeyFrame> keyframes;
	TransformedPattern(std::string pattern);

	size_t transformPosition(size_t position, keyFrameTransformer keyFrameToInpos, keyFrameTransformer keyFrameToOutPos);
	size_t getLinePos(size_t patternPos);
	size_t getPatternPos(size_t linePos);
	//replace this part of the pattern with a type element.
	void replaceLine(size_t lineStartPos, size_t lineEndPos, std::string replacement = std::string() + argumentChar);
	void replacePattern(size_t patternStartPos, size_t patternEndPos, std::string replacement = std::string() + argumentChar);
	void replaceLocal(size_t patternStartPos, size_t patternEndPos, size_t lineEndPos, std::string replacement);
};