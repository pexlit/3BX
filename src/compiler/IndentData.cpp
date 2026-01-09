#include "IndentData.h"

std::string charName(char c)
{
	switch (c)
	{
	case ' ':
		return "space";
	case '\t':
		return "tab";
	default:
		return "'" + c + std::string("'");
	}
}