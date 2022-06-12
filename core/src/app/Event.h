#pragma once
#include "math/Types.h"

enum class EventType {
   None,
   AppQuit,
   AppLoseFocus,
   AppGetFocus,
   WindowResize,
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
