#pragma once
#include "range.h"
#include <vector>
#include <string>
struct Variable
{
	std::string name;
	//all locations which reference this variable
	std::vector<Range> references;
	
};