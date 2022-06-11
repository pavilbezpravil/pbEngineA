#pragma once

#include "imgui.h"


class ImGuiLayer {
public:
   ImGuiLayer();
   ~ImGuiLayer();

   void NewFrame();
   void EndFrame();
};
