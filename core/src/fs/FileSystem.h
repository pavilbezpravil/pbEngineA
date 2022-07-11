#pragma once

#include <filesystem>

namespace pbe {

   namespace fs = std::filesystem;

   std::string ReadFileAsString(std::string_view filename);

}
