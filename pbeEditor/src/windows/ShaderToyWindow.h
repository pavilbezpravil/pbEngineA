#pragma once

#include "EditorWindow.h"
#include "rend/Renderer.h"

namespace pbe {

   class Texture2D;

   struct ShaderToySettings {
      std::string path = "shaderToy.hlsl";
   };

   class ShaderToyWindow : public EditorWindow {
   public:
      ShaderToyWindow();
      ~ShaderToyWindow();

      void OnWindowUI() override;
      void OnUpdate(float dt) override;

   private:
      ShaderToySettings settings;
      float time = 0;

      Ref<Texture2D> texture;
   };

}
