#pragma once

#include <memory>
#include <vector>

#include "EditorWindow.h"
#include "app/Layer.h"
#include "core/Ref.h"

namespace pbe {

   class InspectorWindow;
   class SceneHierarchyWindow;
   class ViewportWindow;
   class Scene;
   struct Event;


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

      std::vector<std::unique_ptr<EditorWindow>> editorWindows;

      Own<Scene> editorScene;

      SceneHierarchyWindow* sceneHierarchyWindow{};
      InspectorWindow* inspectorWindow{};
      ViewportWindow* viewportWindow{};
   };

}
