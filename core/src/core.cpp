#include "core.h"

#include "imgui.h"
#include <filesystem>
namespace fs = std::filesystem;

void CoreUI() {
   ImGui::Text("CoreUI");

   ImGui::Text("cwd %ls", fs::current_path().c_str());
   ImGui::Text("tmp dir %ls", fs::temp_directory_path().c_str());

   for (auto& file : fs::recursive_directory_iterator(".")) {
      ImGui::Text("cwd %ls", file.path().c_str());
   }
}


namespace core {
   void CoreUI2() {
      ImGui::Text("core::CoreUI2");
      ImGui::Text(__FUNCTION__);
      ImGui::Text(__FUNCDNAME__);

      for (auto& file : fs::recursive_directory_iterator(".")) {
         ImGui::Text("\t%ls", file.path().c_str());
      }
   }
}
