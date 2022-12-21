#pragma once

#include <filesystem>

#include "core/Core.h"

namespace pbe {

   namespace fs = std::filesystem;

   std::string ReadFileAsString(std::string_view filename);

   struct OpenFileDialogCfg {
      std::string name;
      std::string filter;
      bool save = false;
   };

   CORE_API std::string OpenFileDialog(const OpenFileDialogCfg& cfg);

   CORE_API void OpenFileExplorer(const string_view path);

}
