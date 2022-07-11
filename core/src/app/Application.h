#pragma once

#include "core/Common.h"
#include "Event.h"
#include "LayerStack.h"
#include "gui/ImGuiLayer.h"

namespace pbe {

   class Application {
   public:
      NON_COPYABLE(Application);
      Application() = default;
      virtual ~Application() = default;

      virtual void OnInit();
      virtual void OnTerm();
      virtual void OnEvent(Event& e);

      void PushLayer(Layer* layer);
      void PushOverlay(Layer* overlay);

      void Run();

   private:
      bool running = false;
      LayerStack layerStack;
      ImGuiLayer* imguiLayer{};

      bool focused = true;
   };

   extern Application* sApplication;

}
