#pragma once

#include <memory>
#include <vector>

#include "EditorWindow.h"
#include "app/Layer.h"
#include "core/Ref.h"


class InspectorWindow;
class SceneHierarchyWindow;
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

   Scene* GetOpenedScene() const { return scene.get(); }

private:
   std::vector<std::unique_ptr<EditorWindow>> editorWindows;

   Scope<Scene> scene;

   SceneHierarchyWindow* sceneHierarchyWindow{};
   InspectorWindow* inspectorWindow{};
};
