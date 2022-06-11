#pragma once

#include "../math/Types.h"
#include <d3d11.h>


class Window {
public:
   Window(int2 size = {1280, 800});

   ~Window();

   WNDCLASSEX wc{};
   HWND hwnd{};
};


extern Window* sWindow;
