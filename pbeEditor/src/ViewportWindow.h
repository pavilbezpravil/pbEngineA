#pragma once

#include "EditorWindow.h"
#include "core/Ref.h"
#include "rend/Renderer.h"

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

      Scene* scene{};
      RenderCamera camera;
      vec2 cameraAngle{};
      Own<Renderer> renderer;
      Ref<Texture2D> colorTexture;
      Ref<Texture2D> depthTexture;

      bool windowFocused = false;
   };

}
