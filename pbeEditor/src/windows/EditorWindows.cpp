#include "pch.h"
#include "EditorWindows.h"
#include "core/Profiler.h"
#include "gui/Gui.h"
#include "rend/Shader.h"

namespace pbe {

   void ProfilerWindow::OnWindowUI() {
      auto& profiler = Profiler::Get();

      ImGui::Text("Profiler Stats");
      ImGui::Text("Cpu:");
      for (const auto& [name, cpuEvent] : profiler.cpuEvents) {
         if (!cpuEvent.usedInFrame) {
            continue;
         }
         // ImGui::Text("  %s: %.2f %.2f ms", cpuEvent.name.data(), cpuEvent.averageTime.GetAverage(), cpuEvent.averageTime.GetCur());
         ImGui::Text("  %s: %.2f ms", cpuEvent.name.data(), cpuEvent.averageTime.GetAverage());
      }

      ImGui::Text("Gpu:");
      for (const auto& [name, gpuEvent] : profiler.gpuEvents) {
         if (!gpuEvent.usedInFrame) {
            continue;
         }
         // ImGui::Text("  %s: %.2f %.2f ms", gpuEvent.name.data(), gpuEvent.averageTime.GetAverage(), gpuEvent.averageTime.GetCur());
         ImGui::Text("  %s: %.2f ms", gpuEvent.name.data(), gpuEvent.averageTime.GetAverage());
      }
   }

   void ConfigVarsWindow::OnWindowUI() {
      const ConfigVarsMng& configVars = sConfigVarsMng;

      ImGui::Text("Vars:");

      CVarChilds(configVars.GetCVarsRoot());
   }

   void ConfigVarsWindow::CVarChilds(const ConfigVarsMng::CVarChilds& childs) {
      if (childs.cvar) {
         childs.cvar->UI();
      } else {
         for (const auto& child : childs.childs) {
            if (child.IsLeaf()) {
               CVarChilds(child);
            }
            else {
               if (ImGui::TreeNodeEx(child.folderName.c_str())) {
                  CVarChilds(child);
                  ImGui::TreePop();
               }
            }
         }
      }
   }

   void ShaderWindow::OnWindowUI() {
      ShadersWindow();
   }

}
