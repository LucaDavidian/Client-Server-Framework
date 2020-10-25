#include "debug.h"

void Error(const std::string &errorMsg)
{
	PRINT("error: ");
	PRINTLN(errorMsg);

	std::exit(0);
}

void DbgPrint(const std::string &msg)
{
	PRINTLN(msg);
}
