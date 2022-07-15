#pragma once

#include "EditorWindow.h"
#include "app/Layer.h"
#include "core/Ref.h"

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
      Own<Renderer> renderer;
      Ref<Texture2D> colorTexture;
      Ref<Texture2D> depthTexture;

      bool windowFocused = false;
   };

}
