#pragma once

#include "core/Common.h"
#include "Event.h"


class Application {
public:
   NON_COPYABLE(Application);
   Application() = default;
   virtual ~Application() = default;

   virtual void OnInit();
   virtual void OnTerm();
   virtual void OnEvent(Event& e);

   void Run();

   bool running = false;
};

extern Application* sApplication;
