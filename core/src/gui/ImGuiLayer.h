#pragma once

#include "imgui.h"
#include "app/Layer.h"


class ImGuiLayer : public Layer {
public:
   void OnAttach() override;
   void OnDetach() override;
   void OnImGuiRender() override;

   void NewFrame();
   void EndFrame();
   void Render();
};
