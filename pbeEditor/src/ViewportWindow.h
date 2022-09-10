#pragma once

#include "EditorSelection.h"
#include "EditorWindow.h"
#include "core/Ref.h"
#include "rend/Renderer.h"

namespace pbe {

   class Scene;
   class Texture2D;

   struct GizmoCfg {
      int operation = 7; // translate
      int space = 1; // world
      float snap = 1;
   };

   class ViewportWindow : public EditorWindow {
   public:
      // using EditorWindow::EditorWindow;
      ViewportWindow(std::string_view name);

      void OnImGuiRender() override;
      void OnUpdate(float dt) override;

      void Gizmo(const vec2& contentRegion, const vec2& cursorPos);

      Scene* scene{};
      EditorSelection* selection{};
      std::function<void(void)> customHeadFunc; // todo: better name

      RenderCamera camera;
      vec2 cameraAngle{};
      Own<Renderer> renderer;
      CameraContext cameraContext;

      bool enableInput = false;

      GizmoCfg gizmoCfg;

   private:
      bool selectEntityUnderCursor = false;
   };

}
