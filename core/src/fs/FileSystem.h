#pragma once

#include <filesystem>
namespace fs = std::filesystem;

std::string ReadFileAsString(std::string_view filename);
