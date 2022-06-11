#include "FileSystem.h"
#include <fstream>

std::string ReadFileAsString(std::string_view filename) {
   std::ifstream file(filename.data());
   std::stringstream buffer;
   buffer << file.rdbuf();
   return buffer.str();
}
