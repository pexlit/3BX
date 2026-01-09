#include <string>
struct IndentData
{
	std::string indentString{};
	int indentLevel{};
	int maxIndentLevel{};
};

std::string charName(char c);