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

      void OnBefore() override;
      void OnAfter() override;
      void OnWindowUI() override;

      void OnUpdate(float dt) override;

      void OnLostFocus() override;

      void Zoom(Texture2D& image, vec2 center);
      void Gizmo(const vec2& contentRegion, const vec2& cursorPos);

      Scene* scene{};
      EditorSelection* selection{};

      EditorCamera camera;
      Renderer* renderer = {}; // todo:
      RenderContext renderContext;

      bool freeCamera = true;

      GizmoCfg gizmoCfg;

   private:
      bool zoomEnable = false;
      bool cameraMove = false;

      void StartCameraMove();
      void StopCameraMove();
   };

}
