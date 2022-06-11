#pragma once
#include "math/Types.h"

enum class EventType {
   None,
   AppQuit,
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

struct WindowResizeEvent : Event {
   WindowResizeEvent(int2 size) : size(size) {}

   int2 size{};

   EVENT_TYPE(WindowResize)
};
