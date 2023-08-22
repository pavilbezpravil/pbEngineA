#include "pch.h"
#include "SceneHierarchyWindow.h"

#include "EditorSelection.h"
#include "Undo.h"
#include "app/Input.h"
#include "gui/Gui.h"
#include "scene/Component.h"
#include "scene/Utils.h"
#include "typer/Serialize.h"

namespace pbe {

   void SceneHierarchyWindow::DragDropChangeParent(const Entity& entity) {
      if (ui::DragDropTarget ddTarget{ DRAG_DROP_ENTITY }) {
         auto childEnt = *ddTarget.GetPayload<Entity>();
         Undo::Get().SaveForFuture(childEnt); // todo: dont work
         bool changed = childEnt.Get<SceneTransformComponent>().SetParent(entity); // todo: add to pending not in all case
         if (changed) {
            Undo::Get().PushSave();
         }
      }
   }

   void SceneHierarchyWindow::OnWindowUI() {
      if (!pScene) {
         ImGui::Text("No scene");
      } else {
         auto& scene = *pScene;
         // todo: undo on delete entity

         // todo: undo
         if (UI_POPUP_CONTEXT_WINDOW(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            Entity entity = SceneAddEntityMenu(scene, vec3_Zero); // todo: spawnPosHint
            if (entity) {
               ToggleSelectEntity(entity);
            }
         }

         UIEntity(pScene->GetRootEntity(), true);

         // place item for drag&drop to root
         // ImGui::SetCursorPosY(0); // todo: mb it help to reorder items in hierarchy
         ImGui::Dummy(ImGui::GetContentRegionAvail());
         DragDropChangeParent(pScene->GetRootEntity());
         if (ImGui::IsItemClicked() && !Input::IsKeyPressing(VK_CONTROL)) {
            selection->ClearSelection();
         }
      }
   }

   void SceneHierarchyWindow::UIEntity(Entity entity, bool sceneRoot) {
      auto& trans = entity.Get<SceneTransformComponent>();
      bool hasChilds = trans.HasChilds();

      const auto* name = std::invoke([&] {
         auto c = entity.TryGet<TagComponent>();
         return c ? c->tag.data() : "Unnamed Entity";
         });

      ImGuiTreeNodeFlags nodeFlags =
         (selection->IsSelected(entity) ? ImGuiTreeNodeFlags_Selected : 0)
         | (sceneRoot ? ImGuiTreeNodeFlags_DefaultOpen : 0)
         | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
         | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap
         // | ImGuiTreeNodeFlags_FramePadding
         | (hasChilds ? 0 : ImGuiTreeNodeFlags_Leaf);

      bool enabled = entity.Enabled();
      if (!enabled) {
         ImGui::PushStyleColor(ImGuiCol_Text, { 0.5f, 0.5f, 0.5f, 1.f });
      }

      auto id = (void*)(uint64)entity.GetUUID();
      ui::TreeNode treeNode{ id, nodeFlags, name };

      if (!enabled) {
         ImGui::PopStyleColor();
      }

      DragDropChangeParent(entity);

      if (!sceneRoot) {
         // if (ImGui::IsItemHovered()) {
         //    ImGui::SetTooltip("Right-click to open popup %s", name);
         // }

         if (UI_DRAG_DROP_SOURCE(DRAG_DROP_ENTITY, entity)) {
            ImGui::Text("Drag&Drop Entity %s", name);
         }

         static bool allowToggleEntity = true;

         if (ImGui::IsItemClicked() && ImGui::IsItemToggledOpen()) {
            // if click was on arrow - do not toggle entity while release mouse and click again
            allowToggleEntity = false;
         }
         if (allowToggleEntity && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            // toggle entity on mouse release in case click was not on arrow
            ToggleSelectEntity(entity);
         }
         if (ImGui::IsItemDeactivated()) {
            // on release allow toggle entity again
            // todo: mb it leads to some bugs when this code dont execute buy some reason
            allowToggleEntity = true;
         }

         if (UI_POPUP_CONTEXT_ITEM()) {
            ImGui::MenuItem(name, nullptr, false, false);

            ImGui::Separator();

            Entity addedEntity = SceneAddEntityMenu(*pScene, vec3_Zero); // todo: spawnPosHint
            if (addedEntity) {
               ToggleSelectEntity(addedEntity);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete")) {
               // todo: add to pending
               Undo::Get().Delete(entity);
               if (selection) {
                  selection->Unselect(entity);
               }
               pScene->DestroyImmediate(entity);
            }

            if (ImGui::MenuItem("Unparent")) {
               trans.SetParent();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save to file")) {
               Serializer ser;
               EntitySerialize(ser, entity);
               // ser.SaveToFile(std::format("{}.yaml", entity.GetName()));
               ser.SaveToFile("entity.yaml");
            }
            if (ImGui::MenuItem("Load from file")) {
               auto deser = Deserializer::FromFile("entity.yaml");
               EntityDeserialize(deser, *entity.GetScene());
            }
         }

         {
            UI_PUSH_ID(id);

            float heightLine = ImGui::GetTextLineHeight();

            ImGui::SameLine(ImGui::GetContentRegionMax().x - heightLine * 0.5f - 10);

            UI_PUSH_STYLE_VAR(ImGuiStyleVar_FramePadding, ImVec2{});

            if (ImGui::Checkbox("##Enable", &enabled)) {
            // if (ImGui::Button("##Enable", { heightLine , heightLine })) {
               entity.EnableToggle();
            }
            if (ImGui::IsItemHovered()) {
               ImGui::SetTooltip(enabled ? "Disable" : "Enable");
            }
         }
      }

      if (treeNode) {
         for (auto child : trans.children) {
            UIEntity(child);
         }
      }
   }

   void SceneHierarchyWindow::ToggleSelectEntity(Entity entity) {
      if (selection) {
         selection->ToggleSelect(entity);
      }
   }

}
