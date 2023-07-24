#pragma once

#include "EditorCamera.h"
#include "EditorSelection.h"
#include "EditorWindow.h"
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
      ~ViewportWindow();

      void OnImGuiRender() override;
      void OnUpdate(float dt) override;

      void Zoom(Texture2D& image, vec2 center);
      void Gizmo(const vec2& contentRegion, const vec2& cursorPos);

      Scene* scene{};
      EditorSelection* selection{};
      std::function<void(void)> customHeadFunc; // todo: better name

      EditorCamera camera;
      Renderer* renderer = {}; // todo:
      RenderContext renderContext;

      bool enableInput = false;
      bool freeCamera = true;

      GizmoCfg gizmoCfg;

   private:
      bool selectEntityUnderCursor = false;
      bool zoomEnable = false;
   };

}
