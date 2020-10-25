#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include <string>

#define PRINT(s)      std::cout << (s) 
#define PRINTLN(s)    PRINT(s) << std::endl

void Error(const std::string &errorMsg);
void DbgPrint(const std::string &msg);

#endif  // DEBUG_H