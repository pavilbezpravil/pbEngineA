#pragma once

#include "core/Common.h"
#include "Event.h"
#include "LayerStack.h"
#include "gui/ImGuiLayer.h"

namespace pbe {

   class CORE_API Application {
   public:
      NON_COPYABLE(Application);
      Application() = default;
      virtual ~Application() = default;

      virtual void OnInit();
      virtual void OnTerm();
      virtual void OnEvent(Event& event);

      void PushLayer(Layer* layer);
      void PushOverlay(Layer* overlay);

      void Run();

      const char* GetBuildType();

   private:
      bool running = false;
      LayerStack layerStack;
      ImGuiLayer* imguiLayer{};

      bool focused = true;
   };

   extern CORE_API Application* sApplication;

}
