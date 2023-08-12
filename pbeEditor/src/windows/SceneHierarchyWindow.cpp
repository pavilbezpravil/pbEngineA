#include "pch.h"
#include "SceneHierarchyWindow.h"

#include "EditorSelection.h"
#include "Undo.h"
#include "app/Input.h"
#include "gui/Gui.h"
#include "math/Random.h"
#include "physics/PhysicsScene.h"
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
            if (ImGui::MenuItem("Create Empty")) {
               ToggleSelectEntity(CreateEmpty(scene));
            }

            if (UI_MENU("Geometry")) {
               // todo: add just geom without physics
            }

            if (UI_MENU("Physics")) {
               if (ImGui::MenuItem("Dynamic Cube")) {
                  ToggleSelectEntity(CreateCube(scene));
               }
               if (ImGui::MenuItem("Static Cube")) {
                  ToggleSelectEntity(CreateCube(scene, CubeDesc{ .dynamic = false }));
               }
               if (ImGui::MenuItem("Trigger")) {
                  ToggleSelectEntity(CreateTrigger(scene));
               }
            }

            if (UI_MENU("Lighting")) {
               if (ImGui::MenuItem("Create Direct Light")) {
                  ToggleSelectEntity(CreateDirectLight(scene));
               }
               if (ImGui::MenuItem("Create Sky")) {
                  ToggleSelectEntity(CreateSky(scene));
               }
            }

            if (UI_MENU("Presets")) {
               vec3 cubeSize{ 25, 10, 25 };

               if (ImGui::MenuItem("Random Cubes")) {
                  Entity root = scene.Create("Random Cubes");

                  for (int i = 0; i < 500; ++i) {
                     CreateCube(scene, CubeDesc{
                        .parent = root,
                        .pos = Random::Float3(-cubeSize, cubeSize),
                        .scale = Random::Float3(vec3{ 0 }, vec3{ 3.f }),
                        .rotation = Random::Float3(vec3{ 0 }, vec3{ 30.f }), // todo: PI?
                        .color = Random::Color() });
                  }

                  ToggleSelectEntity(root);
               }

               if (ImGui::MenuItem("Random Lights")) {
                  Entity root = scene.Create("Random Lights");

                  for (int i = 0; i < 8; ++i) {
                     Entity e = CreateEmpty(scene, std::format("Light {}", i),
                        root, Random::Float3(-cubeSize, cubeSize));

                     auto& light = e.Add<LightComponent>();
                     light.color = Random::Float3(vec3{ 0 }, vec3{ 1.f });
                     light.intensity = Random::Float(0.f, 20.f);
                     light.radius = Random::Float(3, 10);
                  }

                  ToggleSelectEntity(root);
               }

               if (ImGui::MenuItem("Add Geom if Material is presents")) {
                  auto view = scene.View<MaterialComponent>();
                  for (auto _e : view) {
                     Entity e{ _e, &scene };
                     auto& geom = e.GetOrAdd<GeometryComponent>();
                     geom.type = GeomType::Box;
                  }
               }

               if (ImGui::MenuItem("Create wall")) {
                  Entity root = scene.Create("Wall");

                  int size = 10;
                  for (int y = 0; y < size; ++y) {
                     for (int x = 0; x < size; ++x) {
                        CreateCube(scene, CubeDesc{
                           .parent = root,
                           .pos = vec3{ -size / 2.f + x, y + 0.5f, 0 },
                           .color = Random::Color() });
                     }
                  }

                  ToggleSelectEntity(root);
               }

               if (ImGui::MenuItem("Create stack tri")) {
                  Entity root = scene.Create("Stack tri");

                  int size = 10;
                  for (int y = 0; y < size; ++y) {
                     int width = size - y;
                     for (int x = 0; x < width; ++x) {
                        CreateCube(scene, CubeDesc{
                           .parent = root,
                           .pos = vec3{ -width / 2.f + x, y + 0.5f, 0 },
                           .color = Random::Color() });
                     }
                  }

                  ToggleSelectEntity(root);
               }

               if (ImGui::MenuItem("Create stack")) {
                  Entity root = scene.Create("Stack");

                  int size = 10;
                  for (int i = 0; i < size; ++i) {
                     CreateCube(scene, CubeDesc{
                        .parent = root,
                        .pos = vec3{0, i + 0.5f, 0},
                        .color = Random::Color() });
                  }

                  ToggleSelectEntity(root);
               }

               if (ImGui::MenuItem("Create chain")) {
                  Entity root = CreateCube(scene, CubeDesc{
                     .namePrefix = "Chain",
                     .pos = vec3{0, 10, 0},
                     .dynamic = false,
                     .color = Random::Color() });

                  Entity prev = root;
                  int size = 10;
                  for (int i = 0; i < size - 1; ++i) {
                     Entity cur = CreateCube(scene, CubeDesc{
                        .parent = root,
                        .pos = vec3{0, i * 2 + 0.5f, 0},
                        .color = Random::Color() });

                     // todo:
                     CreateDistanceJoint(prev, cur);
                     prev = cur;
                  }

                  ToggleSelectEntity(root);
               }
            }

            if (ImGui::MenuItem("Create Decal")) {
               auto createdEntity = scene.Create();
               createdEntity.Add<DecalComponent>();
               ToggleSelectEntity(createdEntity);
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
         | ImGuiTreeNodeFlags_SpanAvailWidth
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
         bool clearPrevSelection = !Input::IsKeyPressing(VK_CONTROL);
         selection->ToggleSelect(entity, clearPrevSelection);
      }
   }

}
