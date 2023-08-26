#pragma once
#include "KeyCode.h"
#include "math/Types.h"

namespace pbe {

   enum class EventType {
      None,
      AppQuit,
      AppLoseFocus,
      AppGetFocus,
      WindowResize,
      KeyDown,
      KeyUp,
      Mouse,
   };

#define EVENT_TYPE(type) \
   static EventType GetStaticEventType() { return EventType::type; } \
   EventType GetEventType() override { return EventType::type; }


   struct Event {
      bool handled = false;

      template<typename T>
      T* GetEvent() {
         if (GetEventType() == T::GetStaticEventType()) {
            return (T*)this;
         }
         return nullptr;
      }

      virtual EventType GetEventType() = 0;
   };

   struct AppQuitEvent : Event {
      EVENT_TYPE(AppQuit)
   };

   struct AppLoseFocusEvent : Event {
      EVENT_TYPE(AppLoseFocus)
   };

   struct AppGetFocusEvent : Event {
      EVENT_TYPE(AppGetFocus)
   };

   struct WindowResizeEvent : Event {
      WindowResizeEvent(int2 size) : size(size) {}

      int2 size{};

      EVENT_TYPE(WindowResize)
   };

   struct KeyDownEvent : Event {
      KeyDownEvent(KeyCode keyCode) : keyCode(keyCode) {}

      KeyCode keyCode = KeyCode::None;

      EVENT_TYPE(KeyDown)
   };

   struct KeyUpEvent : Event {
      KeyUpEvent(KeyCode keyCode) : keyCode(keyCode) {}

      KeyCode keyCode = KeyCode::None;

      EVENT_TYPE(KeyUp)
   };

   // // want to see dif in code between X Pressed\Release Event and just X Event
   // struct MouseEvent : Event {
   //    MouseEvent(int keyCode, bool pressed) : keyCode(keyCode), pressed(pressed) {}
   //
   //    int keyCode = -1;
   //    bool pressed{};
   //
   //    EVENT_TYPE(KeyReleased)
   // };

}
