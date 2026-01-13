#include "parseContext.h"
#include <iostream>

void ParseContext::reportDiagnostics()
{
	for(Diagnostic d : diagnostics){
		//report all diagnostics with cerr

		std::cerr << d.toString() << "\n";
	}
}