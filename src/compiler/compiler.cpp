#include "compiler.h"
#include <regex>
#include "fileFunctions.h"
#include "stringFunctions.h"
#include "sourceFile.h"
#include "IndentData.h"

// non-capturing regex for line terminators
const std::regex lineTerminatorRegex("/(?<=\r\n|\r(?!\n)|\n)/g");

bool importSourceFile(const std::string &path, ParseContext &context)
{
	std::string text;

	if (!readStringFromFile(path, text))
	{
		return false;
	}

	// split on any line ending and capture the ending too
	std::vector<std::string> lines = splitString(text, lineTerminatorRegex);

	SourceFile *sourceFile = new SourceFile(path, text);

	for (const std::string &lineString : lines)
	{
		CodeLine line = CodeLine(lineString, sourceFile);
		// first, remove comments and trim whitespace from the right

		line.rightTrimmedText = std::regex_replace(lineString, std::regex("#.*$"), "");

		// check if the line is an import statement
		if (line.rightTrimmedText.starts_with("import "))
		{
			// if so, recursively import the file
			// extract the file path
			std::string importPath = line.rightTrimmedText.substr(std::string_view("import ").length());
			if (!importSourceFile(importPath, context))
			{
				return false;
			}
		}
		context.codeLines.push_back(line);
	}
	return true;
}

// step 2: analyze sections
bool analyzeSections(ParseContext &context)
{
	IndentData data{};
	Section currentSection;
	// code lines are added in import order, meaning lines get replaced with code from imported files. we assume that the indent level of the code of imported files and the import statements both match.
	for (CodeLine &line : context.codeLines)
	{
		// check if this line starts a section
		if (line.rightTrimmedText.ends_with(":"))
		{
			// determine the section type
			std::size_t spaceIndex = line.rightTrimmedText.find(' ');
			SectionType sectionType = SectionType::Custom;
			if (spaceIndex != std::string::npos)
			{
				std::string sectionTypeString = line.rightTrimmedText.substr(0, spaceIndex);
				sectionType = sectionTypeFromString(sectionTypeString);
			}
			line.section = new Section(sectionType);
		}
		// check indent level
		std::string indentString = std::regex_match(line.fullText, std::regex("^(\\s*)"))[1];
		if (data.indentString.empty())
		{
			data.indentString = indentString;
			data.indentLevel = !indentString.empty();
		}
		else
		{
			// check if the indentation is valid

			// check type of indent. indentation is only important for section exits, since colons determine section starts.
			if (indentString.length())
			{
				char expectedIndentChar = data.indentString[0];
				size_t invalidCharIndex = indentString.find_first_not_of(expectedIndentChar);
				if (invalidCharIndex != std::string::npos)
				{
					context.diagnostics.push_back(Diagnostic(Diagnostic::Level::Error, "Invalid indentation! expected only " + charName(expectedIndentChar) + "s, but found a " + charName(indentString[invalidCharIndex]), line.sourceFile));
				}
				// check amount of indents
				else if (indentString.length() % data.indentString.length() != 0)
				{
					context.diagnostics.push_back(Diagnostic(Diagnostic::Level::Error, "Invalid indentation! expected " + std::to_string(data.indentString.length() * data.indentLevel) + " " + charName(data.indentString[0]) + "s, but found " + std::to_string(indentString.length())), line.sourceFile);
				}
				else
				{
					int newIndentLevel = indentString.length() / data.indentString.length();
					
					if(newIndentLevel > data.maxIndentLevel){
						//cannot go up sections twice in a time
						context.diagnostics.push_back(Diagnostic(Diagnostic::Level::Error, "Invalid indentation! expected at max " + std::to_string(data.indentString.length() * data.maxIndentLevel) + " " + charName(expectedIndentChar) + "s, but found" + std::to_string(indentString.length()), line.sourceFile));
					}
					data.indentLevel = newIndentLevel;
				}
			}
			else
			{
				data.indentString = "";
				data.indentLevel = 0;
			}
		}

		previousLine = line;
	}

	return true;
}