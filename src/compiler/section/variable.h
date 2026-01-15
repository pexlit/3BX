#pragma once
#include "range.h"
#include <string>
#include <vector>
struct Variable {
	Variable(std::string name) : name(name) {}
	std::string name;
	// all locations which reference this variable
	std::vector<Range> references;
};