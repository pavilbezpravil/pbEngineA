#pragma once

#include "EditorWindow.h"

namespace pbe {

   class Scene;
   class Entity;
   struct EditorSelection;

   class SceneHierarchyWindow : public EditorWindow {
   public:
      using EditorWindow::EditorWindow;

      void DragDropChangeParent(const Entity& entity);

      void OnWindowUI() override;

      void UIEntity(Entity entity, bool sceneRoot = false);

      void SetScene(Scene* scene) {
         pScene = scene;
      }

      void ToggleSelectEntity(Entity entity);

      Scene* pScene{};
      EditorSelection* selection{};
   };

}
