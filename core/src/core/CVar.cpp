#include "pch.h"
#include "CVar.h"

namespace pbe {

   ConfigVarsMng sConfigVarsMng;

   CVar::CVar(std::string_view fullPath) : fullPath(fullPath) {
      size_t pos = fullPath.find_last_of('/');
      if (pos != std::string::npos) {
         name = {fullPath.data() + pos + 1};
      } else {
         name = fullPath;
      }
      sConfigVarsMng.AddCVar(this);
   }

   void ConfigVarsMng::AddCVar(CVar* cvar) {
      configVars.push_back(cvar);
      cvarsChanged = true;
   }

   void ConfigVarsMng::NextFrame() {
      if (cvarsChanged) {
         root = {.folderName = "/"};

         for (auto* cvar : configVars) {
            const auto& fullPath = cvar->fullPath;

            CVarChilds* curNode = &root;

            size_t pos = 0;

            do {
               size_t nextPos = fullPath.find_first_of('/', pos);
               if (nextPos != std::string::npos) {
                  auto count = nextPos - pos;
                  std::string_view folderName{ fullPath.data() + pos, count };

                  auto it = std::ranges::find(curNode->childs, folderName, &CVarChilds::folderName);
                  if (it == curNode->childs.end()) {
                     CVarChilds node{ .folderName = {folderName.data(), count} };
                     it = curNode->childs.insert(curNode->childs.end(), std::move(node));
                  }
                  curNode = &*it;

                  pos = nextPos + 1;
               } else {
                  // it's not folder, it's CVar name
                  CVarChilds node{ .folderName = {fullPath.data() + pos}, .cvar = cvar };
                  curNode->childs.push_back(std::move(node));
                  break;
               }
            } while(true);
         }

         auto childSort = [](CVarChilds& node) {
            std::ranges::sort(node.childs, [&](CVarChilds& a, CVarChilds& b) {
               // return strcmp(a.folderName.c_str(), b.folderName.c_str()) == 1;
               return a.folderName.compare(b.folderName) < 0;
               });
         };

         auto childSort2 = [&childSort](CVarChilds& node) {
            childSort(node);

            for (auto& child : node.childs) {
               childSort(child);
            }
         };

         childSort2(root);

         cvarsChanged = false;
      }

      for (auto* cvar : configVars) {
         cvar->NextFrame();
      }
   }

   void CVarValue<bool>::UI() {
      ImGui::Checkbox(name.c_str(), &value);
   }

   void CVarValue<int>::UI() {
      ImGui::InputInt(name.c_str(), &value);
   }

   void CVarValue<uint>::UI() {
      ImGui::InputScalar(name.c_str(), ImGuiDataType_U32, (void*)&value, nullptr, nullptr, "%d");
   }

   void CVarValue<float>::UI() {
      ImGui::InputFloat(name.c_str(), &value);
   }

   void CVarSlider<int>::UI() {
      ImGui::SliderInt(name.c_str(), &value, min, max);
   }

   void CVarSlider<float>::UI() {
      ImGui::SliderFloat(name.c_str(), &value, min, max);
   }

   CVarTrigger::CVarTrigger(std::string_view name): CVar(name) {
   }

   void CVarTrigger::UI() {
      // triggered = ImGui::Button("Trigger");
      if (ImGui::Button("Trigger")) {
         triggered = true;
      }
      ImGui::SameLine();
      ImGui::Text(name.c_str());
   }

   void CVarTrigger::NextFrame() {
      triggered = false;
   }

}
