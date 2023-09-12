#pragma once

#include <memory>
#include <vector>

#include "EditorSelection.h"
#include "EditorWindow.h"
#include "app/Layer.h"
#include "core/Ref.h"

namespace pbe {
   class Renderer;

   class InspectorWindow;
   class SceneHierarchyWindow;
   class ViewportWindow;
   class Scene;
   struct Event;

   struct EditorSettings {
      std::string scenePath;
      vec3 cameraPos;
   };

   class EditorLayer : public Layer {
   public:
      EditorLayer() = default;

      void OnAttach() override;
      void OnDetach() override;
      void OnUpdate(float dt) override;
      void OnImGuiRender() override;
      void OnEvent(Event& event) override;

      void AddEditorWindow(EditorWindow* window, bool showed = false);

      Scene* GetOpenedScene() const { return editorScene.get(); }

   private:
      void SetEditorScene(Own<Scene>&& scene);
      void SetActiveScene(Scene* scene);
      Scene* GetActiveScene();

      void OnPlay();
      void OnPause();
      void OnContinue();
      void OnSteps(int numSteps);
      void OnStop();
      void TogglePlayStop();

      void ReloadDll();
      void UnloadDll();
      HINSTANCE dllHandler = 0;

      std::vector<std::unique_ptr<EditorWindow>> editorWindows;

      Own<Scene> editorScene;
      Own<Scene> runtimeScene;
      EditorSelection editorSelection;
      EditorSettings editorSettings;

      SceneHierarchyWindow* sceneHierarchyWindow{};
      InspectorWindow* inspectorWindow{};
      ViewportWindow* viewportWindow{};

      enum class State {
         Edit,
         Play,
         Pause,
      };

      uint pauseStateNumSteps = UINT_MAX;

      State editorState = State::Edit;

      // todo:
      Own<Renderer> renderer;
   };

}
