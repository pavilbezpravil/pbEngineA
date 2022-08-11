#pragma once

#include <string_view>

#include "core/Core.h"
#include "core/Type.h"

namespace pbe {

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

}
