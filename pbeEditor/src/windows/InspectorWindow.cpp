#include "pch.h"
#include "InspectorWindow.h"

#include "Undo.h"
#include "gui/Gui.h"
#include "scene/Component.h"
#include "EditorSelection.h"

namespace pbe {

   void InspectorWindow::OnWindowUI() {
      Entity entity = selection ? selection->FirstSelected() : Entity{};

      if (!entity.Valid()) {
         ImGui::Text("No entity");
         return;
      }

      Undo::Get().SaveForFuture(entity);

      const auto& typer = Typer::Get();

      bool edited = false;

      auto contentRegionAvail = ImGui::GetContentRegionAvail();

      std::string name = entity.Get<TagComponent>().tag;
      name.reserve(glm::max((int)name.size(), 64));

      if (ImGui::InputText("##Name", name.data(), name.capacity())) {
         entity.Get<TagComponent>().tag = name.c_str();
         edited = true;
      }

      float heightLine = ImGui::GetFrameHeight();

      ImGui::SameLine(contentRegionAvail.x - heightLine * 0.5f);
      if (ImGui::Button("+", { heightLine , heightLine })) {
         ImGui::OpenPopup("Add Component Popup");
      }
      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("Add Component");
      }

      auto processComponentName = [] (const std::string& name) -> std::string {
         // todo: allocation
         std::string result = name;
         auto pos = result.find("Component");
         if (pos != string::npos) {
            result = result.erase(pos);
         }
         return result;
      };

      if (UI_POPUP("Add Component Popup")) {
         for (const auto& ci : typer.components) {
            auto* pComponent = ci.tryGet(entity);
            if (!pComponent) {
               auto processedName = processComponentName(typer.GetTypeInfo(ci.typeID).name);
               if (ImGui::MenuItem(processedName.data())) {
                  ci.getOrAdd(entity);
                  edited = true;
                  ImGui::CloseCurrentPopup();
               }
            }
         }
      }

      if (UI_TREE_NODE("Scene Transform", DefaultTreeNodeFlags() | ImGuiTreeNodeFlags_DefaultOpen)) {
         edited |= EditorUI(entity.GetTransform());
      }

      for (const auto& ci : typer.components) {
         if (auto* pComponent = ci.tryGet(entity)) {
            const auto& ti = typer.GetTypeInfo(ci.typeID);

            auto processedName = processComponentName(ti.name);
            ui::TreeNode treeNode{ processedName.c_str(), DefaultTreeNodeFlags() };

            {
               UI_PUSH_ID((int)ci.typeID);

               ImGui::SameLine(contentRegionAvail.x - heightLine * 0.5f);
               if (ImGui::Button("X", { heightLine , heightLine })) {
                  ci.remove(entity);
               }
               if (ImGui::IsItemHovered()) {
                  ImGui::SetTooltip("Remove Component");
               }
            }

            if (treeNode) {
               if (EditorUI(ci.typeID, (byte*)pComponent)) {
                  if (ci.onChanged) {
                     ci.onChanged(pComponent);
                  }
                  edited = true;
               }
            }
         }
      }

      if (edited) {
         Undo::Get().PushSave();
      }
   }

}
