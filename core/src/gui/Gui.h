#pragma once

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
