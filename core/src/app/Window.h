#pragma once

#include "../math/Types.h"
#include <d3d11.h>
#include <functional>

struct Event;


class Window {
public:
   Window(int2 size = {1280, 800});

   ~Window();

   void Update();

   WNDCLASSEX wc{};
   HWND hwnd{};

   using EventCallbackFn = std::function<void(Event&)>;
   EventCallbackFn eventCallback;
};


extern Window* sWindow;
