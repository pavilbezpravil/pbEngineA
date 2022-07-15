#include "pch.h"
#include "Input.h"

#include "Event.h"
#include "Window.h"


namespace pbe {

   static Input sInput;

   Input::Input() {
      keyPressed.resize(256); //todo:
   }

   int2 Input::GetMousePosition() {
      return sInput.mousePos;
   }

   int2 Input::GetMouseDelta() {
      if (sInput.mousePosValid) {
         return sInput.mousePos - sInput.mousePosPrev;
      } else {
         return {};
      }
   }

   bool Input::IsKeyPressed(int keyCode) {
      return sInput.keyPressed[keyCode];
   }

   void Input::OnEvent(Event& event) {
      if (auto e = event.GetEvent<KeyPressedEvent>()) {
         sInput.keyPressed[e->keyCode] = true;
      }
      if (auto e = event.GetEvent<KeyReleasedEvent>()) {
         sInput.keyPressed[e->keyCode] = false;
      }

      // if (auto e = event.GetEvent<MouseEvent>()) {
      //    sInput.keyPressed[e->keyCode] = e->pressed;
      // }
   }

   void Input::OnUpdate(float dt) {
      sInput.mousePosPrev = sInput.mousePos;
      sInput.mousePos = sWindow->GetMousePosition();
      if (!sInput.mousePosValid) {
         sInput.mousePosPrev = sInput.mousePos;
      }
      sInput.mousePosValid = true;

      sInput.keyPressed[VK_LBUTTON] = GetAsyncKeyState(VK_LBUTTON);
      sInput.keyPressed[VK_RBUTTON] = GetAsyncKeyState(VK_RBUTTON);
   }

}
