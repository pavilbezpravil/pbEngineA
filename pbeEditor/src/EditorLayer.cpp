#include "pch.h"
#include "EditorLayer.h"
#include "EditorWindow.h"
#include "Undo.h"
#include "app/Application.h"
#include "app/Event.h"
#include "app/Input.h"
#include "app/Window.h"
#include "core/Profiler.h"
#include "core/Type.h"
#include "fs/FileSystem.h"
#include "gui/Gui.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Utils.h"

#include "typer/Serialize.h"
#include "typer/Registration.h"

#include "windows/EditorWindows.h"
#include "windows/ViewportWindow.h"
#include "windows/SceneHierarchyWindow.h"
#include "windows/InspectorWindow.h"
#include "windows/ShaderToyWindow.h"


namespace pbe {

   constexpr char editorSettingPath[] = "editor.yaml";

   TYPER_BEGIN(EditorSettings)
      TYPER_FIELD(scenePath)
      TYPER_FIELD(cameraPos)
   TYPER_END()

   void EditorLayer::OnAttach() {
      Deserialize(editorSettingPath, editorSettings);

      ImGui::SetCurrentContext(GetImGuiContext());

      ReloadDll();

      AddEditorWindow(new ConfigVarsWindow("ConfigVars"), true);
      AddEditorWindow(new ShaderToyWindow());
      AddEditorWindow(sceneHierarchyWindow = new SceneHierarchyWindow("SceneHierarchy"), true);
      AddEditorWindow(inspectorWindow = new InspectorWindow("Inspector"), true);
      AddEditorWindow(viewportWindow = new ViewportWindow("Viewport"), true);
      AddEditorWindow(new ProfilerWindow("Profiler"), true);
      AddEditorWindow(new ShaderWindow("Shader"), true);

      sceneHierarchyWindow->selection = &editorSelection;
      viewportWindow->selection = &editorSelection;
      inspectorWindow->selection = &editorSelection;

      // todo:
      renderer.reset(new Renderer());
      renderer->Init();
      viewportWindow->renderer = renderer.get();

      if (!editorSettings.scenePath.empty()) {
         auto s = SceneDeserialize(editorSettings.scenePath);
         SetEditorScene(std::move(s));
      }

      sWindow->SetTitle(std::format("pbe Editor {}", sApplication->GetBuildType()));

      Layer::OnAttach();
   }

   void EditorLayer::OnDetach() {
      ImGui::SetCurrentContext(nullptr);

      runtimeScene = {};
      editorScene = {};
      UnloadDll();

      Serialize(editorSettingPath, editorSettings);

      Layer::OnDetach();
   }

   void EditorLayer::OnUpdate(float dt) {
      for (auto& window : editorWindows) {
         if (window->show) {
            window->OnUpdate(dt);
         }
      }

      if (auto pScene =GetActiveScene()) {
         pScene->OnTick();
      }

      if (editorState == State::Play && runtimeScene) {
         runtimeScene->OnUpdate(dt);
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
      } else {
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

      UI_WINDOW("DockSpace Demo", &p_open, window_flags);
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

      static bool showImGuiWindow = false;

      if (UI_MENU_BAR()) {
         bool canChangeScene = !runtimeScene;

         if (UI_MENU("File")) {
            if (ImGui::MenuItem("New Scene", nullptr, false, canChangeScene)) {
               editorSettings.scenePath = {};

               Own<Scene> scene = std::make_unique<Scene>();
               CreateDirectLight(*scene);
               CreateSky(*scene);
               CreateCube(*scene, CubeDesc{ .namePrefix = "Ground", .pos = vec3{0, -0.5, 0},
                  .scale = vec3{100, 1, 100}, .dynamic = false, .color = vec3{0.2, 0.6, 0.2} });
               CreateCube(*scene, CubeDesc{ .namePrefix = "Cube", .pos = vec3{0, 3, 0} });

               SetEditorScene(std::move(scene));
            }

            if (ImGui::MenuItem("Open Scene", nullptr, false, canChangeScene)) {
               auto path = OpenFileDialog({ "Scene", "*.scn" });
               if (!path.empty()) {
                  editorSettings.scenePath = path;
                  auto s = SceneDeserialize(editorSettings.scenePath);
                  SetEditorScene(std::move(s));
               }
            }

            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false,
               canChangeScene && !editorSettings.scenePath.empty())) {
               if (editorScene) {
                  SceneSerialize(editorSettings.scenePath, *editorScene);
               }
            }

            if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S", false,
               canChangeScene && !!editorScene)) {
               auto path = OpenFileDialog({ "Scene", "*.scn", true });
               if (!path.empty()) {
                  editorSettings.scenePath = path;
                  SceneSerialize(editorSettings.scenePath, *editorScene);
               }
            }
         }

         if (UI_MENU("Windows")) {
            for (auto& window : editorWindows) {
               ImGui::MenuItem(window->name.c_str(), NULL, &window->show);
            }

            ImGui::MenuItem("ImGuiDemoWindow", NULL, &showImGuiWindow);
         }

         {
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.5f);

            switch (editorState) {
               case State::Edit:
                  if (ImGui::Button("Play") && editorScene) {
                     OnPlay();
                  }
                  break;
               case State::Play:
                  if (ImGui::Button("Stop")) {
                     OnStop();
                  }
                  break;
               default: UNIMPLEMENTED();
            }

            // auto color = editorState == State::Edit ? ImVec4{1, 1, 1, 1} : ImVec4{0, 1, 0, 1};
            // if (ImGui::Button("Play2", color, ImGuiColorEditFlags_NoSmallPreview)) {
            //    
            // }

            if (editorState == State::Edit) {
               ImGui::SameLine();
               if (ImGui::Button("Reload dll")) {
                  ReloadDll();
               }
            }
         }
      }

      if (showImGuiWindow) {
         ImGui::ShowDemoWindow(&showImGuiWindow);
      }

      for (auto& window : editorWindows) {
         if (!window->show) {
            continue;
         }

         window->OnBefore();
         if (UI_WINDOW(window->name.c_str(), &window->show)) {
            window->OnWindowUI();
         }
         window->OnAfter();
      }
   }

   void EditorLayer::OnEvent(Event& event) {
      if (auto* e = event.GetEvent<KeyDownEvent>()) {
         if (e->keyCode == VK_ESCAPE) {
            editorSelection.ClearSelection();
         }

         if (e->keyCode == VK_DELETE) {
            for (auto entity : editorSelection.selected) {
               Undo::Get().Delete(entity); // todo: undo not each but all at once
               GetActiveScene()->DestroyImmediate(entity);
            }
            editorSelection.ClearSelection();
         }

         if (Input::IsKeyPressing(VK_CONTROL)) {
            if (e->keyCode == 'P') {
               TogglePlayStop();
            }

            if (e->keyCode == 'D') {
               auto prevSelected = editorSelection.selected;
               editorSelection.ClearSelection();

               for (auto entity : prevSelected) {
                  auto duplicatedEntity = GetActiveScene()->Duplicate(entity);
                  editorSelection.Select(duplicatedEntity, false);
               }
            }

            if (e->keyCode == 'Z') {
               // todo: dont work
               // Undo::Get().PopAction();
            }

            // test message box
            if (e->keyCode == 'M') {
               MessageBox(sWindow->hwnd, "Test", "MsgBox Window", MB_OK);
            }
         }
      }
   }

   void EditorLayer::AddEditorWindow(EditorWindow* window, bool showed) {
      window->show = showed;
      editorWindows.emplace_back(window);
   }

   void EditorLayer::SetEditorScene(Own<Scene>&& scene) {
      editorScene = std::move(scene);
      SetActiveScene(editorScene.get());
   }

   void EditorLayer::SetActiveScene(Scene* scene) {
      editorSelection.ClearSelection();

      sceneHierarchyWindow->SetScene(scene);
      viewportWindow->scene = scene;
   }

   Scene* EditorLayer::GetActiveScene() {
      switch (editorState) {
      case State::Edit:
         return editorScene.get();
      case State::Play:
         return runtimeScene.get();
      default: UNIMPLEMENTED();
      }
      return nullptr;
   }

   void EditorLayer::OnPlay() {
      runtimeScene = editorScene->Copy();
      SetActiveScene(runtimeScene.get());
      // viewportWindow->freeCamera = false;
      runtimeScene->OnStart();
      editorState = State::Play;
   }

   void EditorLayer::OnStop() {
      runtimeScene->OnStop();
      runtimeScene = {};
      SetActiveScene(editorScene.get());
      editorState = State::Edit;
   }

   void EditorLayer::TogglePlayStop() {
      if (editorState == State::Edit) {
         OnPlay();
      } else if (editorState == State::Play) {
         OnStop();
      } else {
         UNIMPLEMENTED();
      }
   }

   void EditorLayer::ReloadDll() {
      auto loadDll = [&] {
         ASSERT(dllHandler == 0);

         string dllName = "testProj.dll"; // todo:

         if (1) {
            // todo: for hot reload dll. windows lock dll for writing
            fs::copy_file(dllName, "testProjCopy.dll", std::filesystem::copy_options::update_existing);
            dllHandler = LoadLibrary("testProjCopy.dll");
         } else {
            dllHandler = LoadLibrary(dllName.data());
         }

         // process new types
         Typer::Get().Finalize();

         if (!dllHandler) {
            WARN("could not load the dynamic library testProj.dll");
         }
      };

      if (dllHandler) {
         if (editorScene) {
            // todo: do it on RAM
            INFO("Serialize editor scene for dll reload");
            SceneSerialize("dllReload.scn", *editorScene);

            UnloadDll();
            loadDll();

            INFO("Deserialize editor scene for dll reload");
            auto reloadedScene = SceneDeserialize("dllReload.scn");
            SetEditorScene(std::move(reloadedScene));
         } else {
            UnloadDll();
            loadDll();
         }
      } else {
         loadDll();
      }
   }

   void EditorLayer::UnloadDll() {
      if (dllHandler) {
         INFO("Unload game dll");
         FreeLibrary(dllHandler);
         dllHandler = 0;
      }
   }

}
