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

   STRUCT_BEGIN(EditorSettings)
      STRUCT_FIELD(scenePath)
      STRUCT_FIELD(cameraPos)
   STRUCT_END()

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

      if (auto pScene = GetActiveScene()) {
         pScene->OnTick();
      }

      if (editorState == State::Play) {
         ASSERT(runtimeScene);
         runtimeScene->OnUpdate(dt);

         if (pauseStateNumSteps != UINT_MAX) {
            --pauseStateNumSteps;
            if (pauseStateNumSteps == 0) {
               OnPause();
            }
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

               CreateCube(*scene, CubeDesc {
                  .namePrefix = "Ground",
                  .pos = vec3{0, -0.5, 0},
                  .scale = vec3{100, 1, 100},
                  .color = vec3{0.2, 0.6, 0.2},
                  .type = CubeDesc::PhysStatic });
               CreateCube(*scene, CubeDesc {
                  .namePrefix = "Cube",
                  .pos = vec3{0, 3, 0},
                  .type = CubeDesc::PhysDynamic });

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

            auto greenColor = ImVec4{ 0, 0.8f, 0, 1 };
            auto redColor = ImVec4{ 0.8f, 0, 0, 1 };

            {
               auto color = editorState == State::Edit ? greenColor : redColor;
               UI_PUSH_STYLE_COLOR(ImGuiCol_Button, color);
               UI_PUSH_STYLE_COLOR(ImGuiCol_ButtonHovered, (color + ImVec4{ 0.1f, 0.1f, 0.1f, 1 }));
               UI_PUSH_STYLE_COLOR(ImGuiCol_ButtonActive, color);

               if (ImGui::Button(editorState == State::Edit ? "Play" : "Stop")) {
                  TogglePlayStop();
               }
            }

            if (editorState == State::Play) {
               // ImGui::BeginDisabled(true);
               // ImGui::EndDisabled();
               if (ImGui::Button("Pause")) {
                  OnPause();
               }
            }

            if (editorState == State::Pause) {
               if (ImGui::Button("Continue")) {
                  OnContinue();
               }
               if (ImGui::Button("Step")) {
                  OnSteps(1);
               }
            }

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

      if (auto pScene = GetActiveScene()) {
         editorSelection.SyncWithScene(*pScene);
      }

      for (auto& window : editorWindows) {
         if (!window->show) {
            continue;
         }

         window->OnBefore();

         if (UI_WINDOW(window->name.c_str(), &window->show)) {
            if (ImGui::IsWindowHovered() && (!ImGui::IsAnyItemActive() || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
               ImGui::SetWindowFocus();
            }

            bool focused = ImGui::IsWindowFocused();
            if (window->focused != focused) {
               window->focused = focused;
               if (focused) {
                  window->OnFocus();
               } else {
                  window->OnLostFocus();
               }
            }

            window->OnWindowUI();
         } else {
            if (window->focused) {
               window->focused = false;
               window->OnLostFocus();
            }
         }

         window->OnAfter();
      }
   }

   void EditorLayer::OnEvent(Event& event) {
      if (auto* e = event.GetEvent<KeyDownEvent>()) {
         // todo: it's inspector and scene hierarchy window logic
         if (e->keyCode == KeyCode::Escape) {
            editorSelection.ClearSelection();
         }

         if (e->keyCode == KeyCode::Delete) {
            for (auto entity : editorSelection.selected) {
               Undo::Get().Delete(entity); // todo: undo not each but all at once
               GetActiveScene()->DestroyImmediate(entity);
            }
            editorSelection.ClearSelection();
         }

         // todo: scene hierarchy window logic
         if (Input::IsKeyPressing(KeyCode::Ctrl)) {
            if (e->keyCode == KeyCode::P) {
               TogglePlayStop();
            }

            if (e->keyCode == KeyCode::Z) {
               // todo: dont work
               // Undo::Get().PopAction();
            }

            // test message box
            if (e->keyCode == KeyCode::M) {
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
      SetActiveScene(scene.get());
      editorScene = std::move(scene);
   }

   void EditorLayer::SetActiveScene(Scene* scene) {
      editorSelection.ChangeScene(*scene);

      sceneHierarchyWindow->SetScene(scene);
      viewportWindow->scene = scene;
   }

   Scene* EditorLayer::GetActiveScene() {
      switch (editorState) {
         case State::Edit:
            return editorScene.get();
         default:
            return runtimeScene.get();
      }
   }

   void EditorLayer::OnPlay() {
      runtimeScene = editorScene->Copy();
      SetActiveScene(runtimeScene.get());
      // viewportWindow->freeCamera = false;
      runtimeScene->OnStart();
      editorState = State::Play;
   }

   void EditorLayer::OnPause() {
      ASSERT(editorState == State::Play);
      pauseStateNumSteps = UINT_MAX;
      editorState = State::Pause;
   }

   void EditorLayer::OnContinue() {
      ASSERT(editorState == State::Pause);
      editorState = State::Play;
   }

   void EditorLayer::OnSteps(int numSteps) {
      ASSERT(editorState == State::Pause);
      if (numSteps <= 0) {
         return;
      }

      pauseStateNumSteps = numSteps;
      OnContinue();
   }

   void EditorLayer::OnStop() {
      SetActiveScene(editorScene.get());
      runtimeScene->OnStop();
      runtimeScene = {};
      editorState = State::Edit;
   }

   void EditorLayer::TogglePlayStop() {
      if (editorState == State::Edit) {
         OnPlay();
      } else {
         OnStop();
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
