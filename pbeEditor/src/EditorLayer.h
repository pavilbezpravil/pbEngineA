#pragma once

#include <memory>
#include <vector>

#include "EditorWindow.h"
#include "app/Layer.h"


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

private:
   std::vector<std::unique_ptr<EditorWindow>> editorWindows;
};
