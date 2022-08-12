#pragma once

#include "imgui.h"
#include "app/Layer.h"

namespace pbe {

   class ImGuiLayer : public Layer {
   public:
      void OnAttach() override;
      void OnDetach() override;
      void OnImGuiRender() override;
      void OnEvent(Event& event) override;

      void NewFrame();
      void EndFrame();
      void Render();
   };

}
