#pragma once

#include "EditorWindow.h"
#include "core/Ref.h"
#include "rend/Renderer.h"
#include "scene/Entity.h"

namespace pbe {
   class Scene;

   class Renderer;
   class Texture2D;

   class ViewportWindow : public EditorWindow {
   public:
      // using EditorWindow::EditorWindow;
      ViewportWindow(std::string_view name);

      void OnImGuiRender() override;
      void OnUpdate(float dt) override;

      void Gizmo(const ImVec2& contentRegion, const ImVec2& cursorPos);

      Scene* scene{};
      Entity selectedEntity;
      RenderCamera camera;
      vec2 cameraAngle{};
      Own<Renderer> renderer;
      CameraContext cameraContext;

      bool windowFocused = false;
   };

}
