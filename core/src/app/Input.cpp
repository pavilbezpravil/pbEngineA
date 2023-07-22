#include "pch.h"
#include "Input.h"

#include "Event.h"
#include "Window.h"
#include "core/Log.h"


namespace pbe {

   static Input sInput;

   static int2 GetGlobalMousePosition() {
      POINT p;
      if (GetCursorPos(&p)) {
         return { p.x, p.y };
      }
      return {};
   }

   Input::Input() {
      int size = 256; // todo:
      keyDown.resize(size);
      keyPressing.resize(size);
      keyUp.resize(size);

      mousePos = GetGlobalMousePosition();
   }

   int2 Input::GetMousePosition() {
      return sInput.mousePos;
   }

   int2 Input::GetMouseDelta() {
      return sInput.mouseDelta;
   }

   bool Input::IsKeyDown(int keyCode) {
      return sInput.keyDown[keyCode];
   }

   bool Input::IsKeyPressing(int keyCode) {
      return sInput.keyPressing[keyCode];
   }

   bool Input::IsKeyUp(int keyCode) {
      return sInput.keyUp[keyCode];
   }

   void Input::OnEvent(Event& event) {
      if (auto e = event.GetEvent<KeyDownEvent>()) {
         sInput.keyDown[e->keyCode] = true;
         sInput.keyPressing[e->keyCode] = true;
      }
      if (auto e = event.GetEvent<KeyUpEvent>()) {
         sInput.keyPressing[e->keyCode] = false;
         sInput.keyUp[e->keyCode] = true;
      }
   }

   void Input::ClearKeys() {
      std::ranges::fill(sInput.keyDown, false);
      std::ranges::fill(sInput.keyUp, false);
   }

   void Input::NextFrame() {
      sInput.mouseDelta = GetGlobalMousePosition() - sInput.mousePos;
      if (sWindow->lockMouse) {
         SetCursorPos(sInput.mousePos.x, sInput.mousePos.y);
      } else {
         sInput.mousePos += sInput.mouseDelta;
      }
   }

}
