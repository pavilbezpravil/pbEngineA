#pragma once

#include <string_view>

#include "core/Core.h"
#include "core/Type.h"

namespace pbe {

   // todo:
   constexpr char DRAG_DROP_ENTITY[] = "DD_ENTITY";

   /*
   #include <string_view>

   #include "imgui.h"

   struct TreeNode {
      TreeNode(std::string_view name, ImGuiTreeNodeFlags flags = 0) {
         opened = ImGui::TreeNodeEx(name.data(), flags);
      }

      ~TreeNode() {
         if (opened) {
            ImGui::TreePop();
         }
      }

      operator bool() const { return opened; }

      bool opened = false;
   };
   */

   CORE_API ImGuiContext* GetImGuiContext();

   CORE_API void EditorUI(std::string_view name, TypeID typeID, byte* value);

   template<typename T>
   void EditorUI(std::string_view name, T& value) {
      const auto typeID = GetTypeID<T>();
      EditorUI(name, typeID, (byte*)&value);
   }

   void UIColorEdit3(const char* name, byte* value);
   void UIColorPicker3(const char* name, byte* value);

   struct UISliderFloat {
      float min = 0;
      float max = 1;

      void operator()(const char* name, byte* value) {
         ImGui::SliderFloat(name, (float*)value, min, max);
      }
   };

}
