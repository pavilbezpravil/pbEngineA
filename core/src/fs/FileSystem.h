#pragma once

#include <filesystem>

namespace pbe {

   namespace fs = std::filesystem;

   std::string ReadFileAsString(std::string_view filename);

   struct OpenFileDialogCfg {
      std::string name;
      std::string filter;
      bool save = false;
   };

   std::string OpenFileDialog(const OpenFileDialogCfg& cfg);

}
