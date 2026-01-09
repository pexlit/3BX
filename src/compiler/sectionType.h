#include <string>
enum class SectionType
{
	Custom,
	Section,
	Expression,
	Effect,
	Class
};

SectionType sectionTypeFromString(const std::string& str);
std::string sectionTypeToString(SectionType type);