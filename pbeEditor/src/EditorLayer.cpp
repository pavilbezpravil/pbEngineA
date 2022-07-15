#include "pch.h"
#include "EditorLayer.h"
#include "EditorWindow.h"
#include "ViewportWindow.h"
#include "app/Event.h"
#include "gui/Gui.h"
#include "math/Random.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "rend/Renderer.h"


namespace pbe {

   class SceneHierarchyWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         if (!pScene) {
            ImGui::Text("No scene");
         }
         else {
            // todo:
            if (ImGui::BeginPopupContextItem()) {
               if (ImGui::Selectable("Create new entity")) {
                  pScene->Create();
               }
               ImGui::EndPopup();
            }

            for (auto [e, tag] : pScene->GetEntitiesWith<TagComponent>().each()) {
               const auto* name = tag.tag.data();

               if (ImGui::TreeNodeEx(tag.tag.data(), ImGuiTreeNodeFlags_SpanFullWidth)) {
                  if (ImGui::BeginPopupContextItem()) {
                     ImGui::Text("This a popup for \"%s\"!", name);

                     if (ImGui::Button("Add component"))
                        ImGui::Text("sfdsdf");

                     ImGui::Separator();

                     if (ImGui::Button("Delete"))
                        ImGui::Text("sfdsdf");

                     if (ImGui::Button("Close"))
                        ImGui::CloseCurrentPopup();

                     ImGui::EndPopup();
                  }

                  // if (ImGui::IsItemHovered()) {
                  //    ImGui::SetTooltip("Right-click to open popup");
                  // }

                  if (ImGui::IsItemClicked()) {
                     if (selectedCb) {
                        Entity entity{ e, pScene };
                        selectedCb(entity);
                     }
                  }

                  ImGui::TreePop();
               }
            }
         }

         ImGui::End();
      }

      void SetScene(Scene* scene) {
         pScene = scene;
         if (selectedCb) {
            selectedCb({});
         }
      }

      Scene* pScene{};

      std::function<void(Entity)> selectedCb;
   };

   class InspectorWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         if (!entity.Valid()) {
            ImGui::Text("No entity");
         } else {
            ImGui::Text("%s %llu", entity.Get<TagComponent>().tag.c_str(), (uint64)entity.Get<UUIDComponent>().uuid);

            for (const auto& [id, func] : ComponentList::Get().components2) {
               func(entity);
            }
         }

         ImGui::End();
      }

      void SetEntity(Entity e) {
         entity = e;
      }

      Entity entity{};
   };

   void EditorLayer::OnAttach() {
      AddEditorWindow(sceneHierarchyWindow = new SceneHierarchyWindow("SceneHierarchy"), true);
      AddEditorWindow(inspectorWindow = new InspectorWindow("Inspector"), true);
      AddEditorWindow(viewportWindow = new ViewportWindow("Viewport"), true);

      sceneHierarchyWindow->selectedCb = [&](Entity e) { inspectorWindow->SetEntity(e); };

      Layer::OnAttach();

      // todo:
      Own<Scene> scene{ new Scene() };

      // scene->Create("red");
      // scene->Create("green");
      // scene->Create("blue");

      for (int i = 0; i < 100; ++i) {
         Entity e = scene->Create(std::to_string(i));

         e.Get<SceneTransformComponent>().position = Random::Uniform({-10, -10, 0},
            {10, 10, 10});
         e.Get<SimpleMaterialComponent>().albedo = Random::Uniform(vec3_Zero, vec3_One);
      }

      SetEditorScene(std::move(scene));
   }

   void EditorLayer::OnDetach() {
      Layer::OnDetach();
   }

   void EditorLayer::OnUpdate(float dt) {
      for (auto& window : editorWindows) {
         if (window->show) {
            window->OnUpdate(dt);
         }
      }
   }

   void EditorLayer::OnImGuiRender() {
      static bool p_open = true; // todo:

      static bool opt_fullscreen = true;
      static bool opt_padding = false;
      static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

      // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
      // because it would be confusing to have two docking targets within each others.
      ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
      if (opt_fullscreen) {
         const ImGuiViewport* viewport = ImGui::GetMainViewport();
         ImGui::SetNextWindowPos(viewport->WorkPos);
         ImGui::SetNextWindowSize(viewport->WorkSize);
         ImGui::SetNextWindowViewport(viewport->ID);
         ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
         ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
         window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove;
         window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
      }
      else {
         dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
      }

      // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
      // and handle the pass-thru hole, so we ask Begin() to not render a background.
      if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
         window_flags |= ImGuiWindowFlags_NoBackground;

      // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
      // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
      // all active windows docked into it will lose their parent and become undocked.
      // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
      // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
      if (!opt_padding)
         ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
      ImGui::Begin("DockSpace Demo", &p_open, window_flags);
      if (!opt_padding)
         ImGui::PopStyleVar();

      if (opt_fullscreen)
         ImGui::PopStyleVar(2);

      // Submit the DockSpace
      ImGuiIO& io = ImGui::GetIO();
      if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
         ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
         ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
      }

      if (ImGui::BeginMenuBar()) {
         if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
               INFO("New Scene");
            }
            if (ImGui::MenuItem("Open Scene")) {
               auto s = SceneDeserialize("scene.scn");
               SetEditorScene(std::move(s));
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
               if (editorScene) {
                  SceneSerialize("scene.scn", *editorScene);
               }
            }

            ImGui::EndMenu();
         }

         ImGui::EndMenuBar();
      }

      if (ImGui::BeginMenuBar()) {
         if (ImGui::BeginMenu("Windows")) {
            for (auto& window : editorWindows) {
               ImGui::MenuItem(window->name.c_str(), NULL, &window->show);
            }
            ImGui::EndMenu();
         }

         ImGui::EndMenuBar();
      }

      ImGui::End();


      for (auto& window : editorWindows) {
         if (window->show) {
            window->OnImGuiRender();
         }
      }
   }

   void EditorLayer::OnEvent(Event& event) {

   }


   void EditorLayer::AddEditorWindow(EditorWindow* window, bool showed) {
      window->show = showed;
      editorWindows.emplace_back(window);
   }

   void EditorLayer::SetEditorScene(Own<Scene>&& scene) {
      editorScene = std::move(scene);

      sceneHierarchyWindow->SetScene(editorScene.get());
      viewportWindow->scene = editorScene.get();
   }

}
