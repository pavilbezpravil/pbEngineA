#pragma once
#include "KeyCode.h"
#include "core/Core.h"
#include "math/Types.h"


namespace pbe {
   struct Event;


   // todo: imgui suppresed some key events

   class CORE_API Input {
   public:
      Input(); // todo:
      // static void Init();

      static int2 GetMousePosition();
      static int2 GetMouseDelta();

      static void LockMousePos(bool lock);
      static void HideMouse(bool lock = false);
      static void ShowMouse(bool unlock = false);

      static bool IsKeyDown(KeyCode keyCode);
      static bool IsKeyPressing(KeyCode keyCode);
      static bool IsKeyUp(KeyCode keyCode);

      static void OnEvent(Event& event);
      static void ClearKeys();
      static void NextFrame();

   private:
      std::vector<bool> keyDown;
      std::vector<bool> keyPressing;
      std::vector<bool> keyUp;

      int2 mousePos{};
      int2 mouseDelta{};
      bool mouseLocked = false;
   };

}
