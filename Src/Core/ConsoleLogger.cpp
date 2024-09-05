#include "ConsoleLogger.h"
#include <iostream>

void ConsoleLogger::Show(const ApplicationInfos& infos) const
{
	for(const auto& info : infos.infos){
		std::cout << info.c_str() << '\n';
	}
	for (const auto& warning : infos.warnings)
	{
		std::cout << "WARNING: " << warning.c_str() << '\n';
	}
	for (const auto& error : infos.errors)
	{
		std::cout << "ERROR: " << error.c_str() << '\n';
	}
}
