#pragma once
#include "core/Core.h"
#include "math/Types.h"


namespace pbe {
   struct Event;


   class CORE_API Input {
   public:
      Input(); // todo:
      // static void Init();

      static int2 GetMousePosition();
      static int2 GetMouseDelta();

      static bool IsKeyPressed(int keyCode);

      static void OnEvent(Event& event);
      static void OnUpdate(float dt);

   private:
      std::vector<bool> keyPressed;

      bool mousePosValid = false;
      int2 mousePos{};
      int2 mousePosPrev{};
   };

}
