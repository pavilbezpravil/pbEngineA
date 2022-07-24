#include "pch.h"
#include "EditorLayer.h"
#include "EditorWindow.h"
#include "ViewportWindow.h"
#include "app/Event.h"
#include "core/Profiler.h"
#include "fs/FileSystem.h"
#include "gui/Gui.h"
#include "math/Random.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "rend/Renderer.h"
#include "typer/Typer.h"


namespace pbe {

   constexpr char editorSettingPath[] = "editor.yaml";

   TYPER_BEGIN(EditorSettings)
      TYPER_FIELD(scenePath)
   TYPER_END(EditorSettings)

   class SceneHierarchyWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         if (!pScene) {
            ImGui::Text("No scene");
         }
         else {
            if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
               if (ImGui::MenuItem("Create Entity")) {
                  auto createdEntity = pScene->Create();
                  SelectEntity(createdEntity);
               }
               ImGui::EndPopup();
            }

            for (auto [e, _] : pScene->GetEntitiesWith<UUIDComponent>().each()) {
               Entity entity{ e, pScene };

               const auto* name = std::invoke([&] {
                  auto c = entity.TryGet<TagComponent>();
                  return c ? c->tag.data() : "Unnamed Entity";
               });

               // const auto& trans = entity.Get<SceneTransformComponent>();
               bool hasChilds = false;

               ImGuiTreeNodeFlags nodeFlags =
                  (entity == selection->FirstSelected() ? ImGuiTreeNodeFlags_Selected : 0)
                  | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                  | ImGuiTreeNodeFlags_SpanFullWidth
                  | (hasChilds ? 0 : ImGuiTreeNodeFlags_Leaf);

               if (ImGui::TreeNodeEx((void*)(size_t)entity.GetID(), nodeFlags, name)) {
                  if (ImGui::IsItemClicked()) {
                     SelectEntity(entity);
                  }

                  if (ImGui::BeginPopupContextItem()) {
                     ImGui::Text("This a popup for \"%s\"!", name);

                     if (ImGui::Button("Add component")) {
                        INFO("Add component");
                     }

                     ImGui::Separator();

                     if (ImGui::Button("Delete")) {
                        // todo: add to pending
                        pScene->DestroyImmediate(entity);
                     }

                     if (ImGui::Button("Close")) {
                        ImGui::CloseCurrentPopup();
                     }

                     ImGui::EndPopup();
                  }

                  // if (ImGui::IsItemHovered()) {
                  //    ImGui::SetTooltip("Right-click to open popup");
                  // }

                  ImGui::TreePop();
               }
            }
         }

         ImGui::End();
      }

      void SetScene(Scene* scene) {
         pScene = scene;
      }

      void SelectEntity(Entity entity) {
         if (selection) {
            selection->Select(entity);
         }
      }

      Scene* pScene{};
      EditorSelection* selection{};

      // std::function<void(Entity)> selectedCb;
   };

   class InspectorWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         Entity entity = selection ? selection->FirstSelected() : Entity{};

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
      
      EditorSelection* selection{};
   };

   class ProfilerWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void OnImGuiRender() override {
         ImGui::Begin(name.c_str(), &show);

         auto& profiler = Profiler::Get();

         ImGui::Text("Profiler Stats");
         ImGui::Text("Cpu:");
         for (const auto& [name, cpuEvent] : profiler.cpuEvents) {
            if (!cpuEvent.usedInFrame) {
               continue;
            }
            // ImGui::Text("  %s: %.2f %.2f ms", cpuEvent.name.data(), cpuEvent.averageTime.GetAverage(), cpuEvent.averageTime.GetCur());
            ImGui::Text("  %s: %.2f ms", cpuEvent.name.data(), cpuEvent.averageTime.GetAverage());
         }

         ImGui::Text("Gpu:");
         for (const auto& [name, gpuEvent] : profiler.gpuEvents) {
            if (!gpuEvent.usedInFrame) {
               continue;
            }
            // ImGui::Text("  %s: %.2f %.2f ms", gpuEvent.name.data(), gpuEvent.averageTime.GetAverage(), gpuEvent.averageTime.GetCur());
            ImGui::Text("  %s: %.2f ms", gpuEvent.name.data(), gpuEvent.averageTime.GetAverage());
         }

         ImGui::End();
      }
   };

   void EditorLayer::OnAttach() {
      // todo:
      if (fs::exists(editorSettingPath)) {
         YAML::Node node = YAML::LoadFile(editorSettingPath);
         Typer::Get().Deserialize(node, "settings", editorSettings);
      }

      AddEditorWindow(sceneHierarchyWindow = new SceneHierarchyWindow("SceneHierarchy"), true);
      AddEditorWindow(inspectorWindow = new InspectorWindow("Inspector"), true);
      AddEditorWindow(viewportWindow = new ViewportWindow("Viewport"), true);
      AddEditorWindow(new ProfilerWindow("Profiler"), true);

      sceneHierarchyWindow->selection = &editorSelection;
      viewportWindow->selection = &editorSelection;
      inspectorWindow->selection = &editorSelection;

      if (!editorSettings.scenePath.empty()) {
         auto s = SceneDeserialize(editorSettings.scenePath);
         SetEditorScene(std::move(s));
      }

      Layer::OnAttach();
   }

   void EditorLayer::OnDetach() {
      // todo:
      {
         YAML::Emitter out;

         out << YAML::BeginMap;
         Typer::Get().Serialize(out, "settings", editorSettings);
         out << YAML::EndMap;

         std::ofstream fout{ editorSettingPath };
         fout << out.c_str();
      }

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
               Own<Scene> scene{ new Scene() };
               SetEditorScene(std::move(scene));
            }

            if (ImGui::MenuItem("Open Scene")) {
               auto path = OpenFileDialog({"Scene", "*.scn"});
               if (!path.empty()) {
                  editorSettings.scenePath = path;
                  auto s = SceneDeserialize(editorSettings.scenePath);
                  SetEditorScene(std::move(s));
               }
            }

            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, !editorSettings.scenePath.empty())) {
               if (editorScene) {
                  SceneSerialize(editorSettings.scenePath, *editorScene);
               }
            }

            if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S", false, !!editorScene)) {
               auto path = OpenFileDialog({ "Scene", "*.scn", true });
               if (!path.empty()) {
                  editorSettings.scenePath = path;
                  SceneSerialize(editorSettings.scenePath, *editorScene);
               }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Scene Random", nullptr, false, !!editorScene)) {
               auto& scene = editorScene;

               {
                  auto e = scene->Create("Red");
                  auto& t = e.Get<SceneTransformComponent>();
                  t.position = { 0, 0, 50 };
                  t.scale = { 100, 100, 1 };
                  auto& m = e.Add<SimpleMaterialComponent>();
                  m.albedo = { 1, 0, 0 };
               }

               {
                  auto e = scene->Create("Green");
                  auto& t = e.Get<SceneTransformComponent>();
                  t.position = { 0, -10, 0 };
                  t.scale = { 100, 1, 100 };
                  auto& m = e.Add<SimpleMaterialComponent>();
                  m.albedo = { 0, 1, 0 };
               }

               {
                  auto e = scene->Create("Blue");
                  auto& t = e.Get<SceneTransformComponent>();
                  t.position = { 50, 0, 0 };
                  t.scale = { 1, 100, 100 };
                  auto& m = e.Add<SimpleMaterialComponent>();
                  m.albedo = { 0, 0, 1 };
               }

               int3 cubeSize{ 25, 10, 25 };

               for (int i = 0; i < 500; ++i) {
                  Entity e = scene->Create(std::format("Cube {}", i));

                  auto& trans = e.GetOrCreate<SceneTransformComponent>();
                  trans.position = Random::Uniform(-cubeSize, cubeSize);
                  trans.scale = Random::Uniform(vec3{ 0 }, vec3{ 3.f });
                  trans.rotation = Random::Uniform(vec3{ 0 }, vec3{ 30.f });

                  auto& material = e.GetOrCreate<SimpleMaterialComponent>();
                  material.albedo = Random::Uniform(vec3_Zero, vec3_One);
                  material.metallic = Random::Uniform(0, 1);
                  material.roughness = Random::Uniform(0, 1);
                  material.opaque = Random::Bool(0.75f);
               }

               for (int i = 0; i < 8; ++i) {
                  Entity e = scene->Create(std::format("Light {}", i));

                  auto& trans = e.GetOrCreate<SceneTransformComponent>();
                  trans.position = Random::Uniform(-cubeSize, cubeSize);

                  auto& light = e.Add<LightComponent>();
                  light.color = Random::Uniform(vec3{ 0 }, vec3{ 20.f });
                  light.radius = Random::Uniform(3, 10);
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
      if (auto* e = event.GetEvent<KeyPressedEvent>()) {
         if (e->keyCode == VK_ESCAPE) {
            editorSelection.ClearSelection();
         }
      }
   }

   void EditorLayer::AddEditorWindow(EditorWindow* window, bool showed) {
      window->show = showed;
      editorWindows.emplace_back(window);
   }

   void EditorLayer::SetEditorScene(Own<Scene>&& scene) {
      editorScene = std::move(scene);
      editorSelection.ClearSelection();

      sceneHierarchyWindow->SetScene(editorScene.get());
      viewportWindow->scene = editorScene.get();
   }

}
