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

   void Input::LockMousePos(bool lock) {
      sInput.mouseLocked = lock;
   }

   void Input::HideMouse(bool lock) {
      while (ShowCursor(FALSE) > 0) {}
      if (lock) {
         LockMousePos(true);
      }
   }

   void Input::ShowMouse(bool unlock) {
      if (unlock) {
         LockMousePos(false);
      }
      while (ShowCursor(TRUE) < 0) {}
   }

   bool Input::IsKeyDown(KeyCode keyCode) {
      return sInput.keyDown[(int)keyCode];
   }

   bool Input::IsKeyPressing(KeyCode keyCode) {
      return sInput.keyPressing[(int)keyCode];
   }

   bool Input::IsKeyUp(KeyCode keyCode) {
      return sInput.keyUp[(int)keyCode];
   }

   void Input::OnEvent(Event& event) {
      if (auto e = event.GetEvent<KeyDownEvent>()) {
         sInput.keyDown[(int)e->keyCode] = true;
         sInput.keyPressing[(int)e->keyCode] = true;
      }
      if (auto e = event.GetEvent<KeyUpEvent>()) {
         sInput.keyPressing[(int)e->keyCode] = false;
         sInput.keyUp[(int)e->keyCode] = true;
      }
   }

   void Input::ClearKeys() {
      std::ranges::fill(sInput.keyDown, false);
      std::ranges::fill(sInput.keyUp, false);
   }

   void Input::NextFrame() {
      sInput.mouseDelta = GetGlobalMousePosition() - sInput.mousePos;
      if (sInput.mouseLocked) {
         SetCursorPos(sInput.mousePos.x, sInput.mousePos.y);
      } else {
         sInput.mousePos += sInput.mouseDelta;
      }
   }

}
