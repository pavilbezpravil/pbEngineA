#pragma once

#include "math/Types.h"
#include "core/Core.h"
#include <functional>

namespace pbe {

   struct Event;

   class CORE_API Window {
   public:
      Window(int2 size = { 1280, 800 });
      ~Window();

      void Update();

      void SetTitle(const string_view title);

      int2 GetMousePosition() const;

      void HideAndLockMouse();
      void ReleaseMouse();

      WNDCLASSEX wc{};
      HWND hwnd{};

      using EventCallbackFn = std::function<void(Event&)>;
      EventCallbackFn eventCallback;
   };

   extern CORE_API Window* sWindow;

}
