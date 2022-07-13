#include "pch.h"
#include "FileSystem.h"

namespace pbe {

   std::string ReadFileAsString(std::string_view filename) {
      std::ifstream file(filename.data());
      std::stringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
   }

}
