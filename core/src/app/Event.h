#pragma once
#include "math/Types.h"

namespace pbe {

   enum class EventType {
      None,
      AppQuit,
      AppLoseFocus,
      AppGetFocus,
      WindowResize,
      KeyPressed,
      KeyReleased,
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

   struct KeyPressedEvent : Event {
      KeyPressedEvent(int keyCode) : keyCode(keyCode) {}

      int keyCode = -1;

      EVENT_TYPE(KeyPressed)
   };

   struct KeyReleasedEvent : Event {
      KeyReleasedEvent(int keyCode) : keyCode(keyCode) {}

      int keyCode = -1;

      EVENT_TYPE(KeyReleased)
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
