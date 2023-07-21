#include "pch.h"
#include "InspectorWindow.h"

#include "Undo.h"
#include "gui/Gui.h"
#include "scene/Component.h"
#include "EditorSelection.h"

namespace pbe {

   void InspectorWindow::OnImGuiRender() {
      UI_WINDOW(name.c_str(), &show);

      Entity entity = selection ? selection->FirstSelected() : Entity{};

      if (!entity.Valid()) {
         ImGui::Text("No entity");
      } else {
         Undo::Get().SaveForFuture(entity);

         bool edited = false;

         // ImGui::Text("%s %llu", entity.Get<TagComponent>().tag.c_str(), (uint64)entity.Get<UUIDComponent>().uuid);

         std::string name = entity.Get<TagComponent>().tag;
         name.reserve(glm::max((int)name.size(), 64));

         ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
         if (ImGui::InputText("Name", name.data(), name.capacity())) {
            entity.Get<TagComponent>().tag = name.c_str();
            edited = true;
         }

         ImGui::SameLine();
         ImGui::Text("0x%jx", (uint64)entity.Get<UUIDComponent>().uuid);

         edited |= EditorUI("Scene Transform", entity.GetTransform());

         const auto& typer = Typer::Get();

         for (const auto& ci : typer.components) {
            if (auto* pComponent = ci.tryGet(entity)) {
               const auto& ti = typer.GetTypeInfo(ci.typeID);
               edited |= EditorUI(ti.name.data(), ci.typeID, (byte*)pComponent);
            }
         }

         if (UI_POPUP_CONTEXT_ITEM("Add Component Popup")) {
            for (const auto& ci : typer.components) {
               auto* pComponent = ci.tryGet(entity);
               if (!pComponent) {
                  auto text = std::format("Add {}", typer.GetTypeInfo(ci.typeID).name);
                  if (ImGui::Button(text.data())) {
                     ci.getOrAdd(entity);
                     edited = true;
                  }
               }
            }
         }

         if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("Add Component Popup");
         }

         if (edited) {
            Undo::Get().PushSave();
         }
      }
   }

}
