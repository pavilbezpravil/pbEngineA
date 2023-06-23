#include "pch.h"
#include "FileSystem.h"

namespace pbe {

   std::string ReadFileAsString(std::string_view filename) {
      if (!fs::exists(filename)) {
         return {};
      }
      std::ifstream file(filename.data());
      std::stringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
   }

   std::string OpenFileDialog(const OpenFileDialogCfg& cfg) {
      OPENFILENAMEA ofn{};
      CHAR szFile[260] = { 0 };

      auto filter = cfg.name + '\0' + cfg.filter + '\0';

      ofn.lStructSize = sizeof(OPENFILENAME);
      // ofn.hwndOwner = ;
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter = filter.data();
      ofn.nFilterIndex = 1;
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

      bool res = cfg.save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
      if (res) {
         return std::filesystem::relative(ofn.lpstrFile).string();
      }
      return {};
   }

   void OpenFileExplorer(const string_view path) {
      ShellExecuteA(NULL, "open", path.data(), NULL, NULL, SW_SHOWDEFAULT);
   }
}
