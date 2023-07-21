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

   void SceneHierarchyWindow::OnImGuiRender() {
      if (!pScene) {
         ImGui::Text("No scene");
      } else {
         // todo: undo on delete entity

         // todo: undo
         if (UI_POPUP_CONTEXT_WINDOW(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty")) {
               ToggleSelectEntity(CreateEmpty(*pScene));
            }
            if (ImGui::MenuItem("Create Dynamic Cube")) {
               ToggleSelectEntity(CreateCube(*pScene));
            }
            if (ImGui::MenuItem("Create Static Cube")) {
               ToggleSelectEntity(CreateCube(*pScene, CubeDesc{ .dynamic = false }));
            }
            if (ImGui::MenuItem("Create Direct Light")) {
               ToggleSelectEntity(CreateDirectLight(*pScene));
            }
            if (ImGui::MenuItem("Create Sky")) {
               ToggleSelectEntity(CreateSky(*pScene));
            }
            if (ImGui::MenuItem("Create Decal")) {
               auto createdEntity = pScene->Create();
               createdEntity.Add<DecalComponent>();
               ToggleSelectEntity(createdEntity);
            }
         }

         UIEntity(pScene->GetRootEntity(), true);

         // place item for drag&drop to root
         // ImGui::SetCursorPosY(0); // todo: mb it help to reorder items in hierarchy
         ImGui::Dummy(ImGui::GetContentRegionAvail());
         DragDropChangeParent(pScene->GetRootEntity());
         if (ImGui::IsItemClicked() && !Input::IsKeyPressed(VK_CONTROL)) {
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

      auto entityUIAction = [&]() {
         if (sceneRoot) {
            return;
         }

         if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Right-click to open popup %s", name);
         }

         if (UI_DRAG_DROP_SOURCE(DRAG_DROP_ENTITY, entity)) {
            ImGui::Text("Drag&Drop Entity %s", name);
         }

         DragDropChangeParent(entity);

         // IsItemToggledOpen: when node opened dont select entity
         if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            ToggleSelectEntity(entity);
         }

         if (UI_POPUP_CONTEXT_ITEM()) {
            ImGui::Text("This a popup for \"%s\"!", name);

            ImGui::Separator();

            if (ImGui::Button("Delete")) {
               // todo: add to pending
               Undo::Get().Delete(entity);
               if (selection) {
                  selection->Unselect(entity);
               }
               pScene->DestroyImmediate(entity);
            }

            if (ImGui::Button("Unparent")) {
               trans.SetParent();
            }

            if (ImGui::Button("Close")) {
               ImGui::CloseCurrentPopup();
            }

            if (ImGui::Button("Save to file")) {
               Serializer ser;
               EntitySerialize(ser, entity);
               // ser.SaveToFile(std::format("{}.yaml", entity.GetName()));
               ser.SaveToFile("entity.yaml");
            }
            if (ImGui::Button("Load from file")) {
               auto deser = Deserializer::FromFile("entity.yaml");
               EntityDeserialize(deser, *entity.GetScene());
            }
         }
      };

      ImGuiTreeNodeFlags nodeFlags =
         (selection->IsSelected(entity) ? ImGuiTreeNodeFlags_Selected : 0)
         | (sceneRoot ? ImGuiTreeNodeFlags_DefaultOpen : 0)
         | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
         | ImGuiTreeNodeFlags_SpanFullWidth
         | (hasChilds ? 0 : ImGuiTreeNodeFlags_Leaf);

      if (UI_TREE_NODE((void*)(uint64)entity.GetUUID(), nodeFlags, name)) {
         entityUIAction();

         for (auto child : trans.children) {
            UIEntity(child);
         }
      } else {
         entityUIAction();
      }
   }

   void SceneHierarchyWindow::ToggleSelectEntity(Entity entity) {
      if (selection) {
         bool clearPrevSelection = !Input::IsKeyPressed(VK_CONTROL);
         selection->ToggleSelect(entity, clearPrevSelection);
      }
   }

}
