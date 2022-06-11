#include "EditorLayer.h"
#include "EditorWindow.h"

#include "imgui.h"


void EditorLayer::OnAttach() {
   Layer::OnAttach();
}

void EditorLayer::OnDetach() {
   Layer::OnDetach();
}

void EditorLayer::OnUpdate(float dt) {
   Layer::OnUpdate(dt);
}

void EditorLayer::OnImGuiRender() {
   for (auto& window: editorWindows) {
      window->OnImGuiRender();
   }
}

void EditorLayer::OnEvent(Event& event) {
   Layer::OnEvent(event);
}


void EditorLayer::AddEditorWindow(EditorWindow* window) {
   editorWindows.emplace_back(window);
}
