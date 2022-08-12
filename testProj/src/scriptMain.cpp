#include "pch.h"

#include "scene/Component.h"
#include "script/NativeScript.h"
#include "typer/Typer.h"


namespace pbe {

   class TestScript : public NativeScript {
   public:
      void OnEnable() override {
         INFO("OnEnable {}", GetName());
      }

      void OnDisable() override {
         INFO("OnDisable {}", GetName());
      }

      void OnUpdate(float dt) override {
         INFO("OnUpdate {}. intVal = {}", dt, intValue);

         if (doMove) {
            auto& tran= owner.Get<SceneTransformComponent>();
            tran.position.x += speed * dt;
         }
      }

      float floatValue = 2;
      int intValue = 2;
      float speed = 1;
      bool doMove = false;
   };

   TYPER_BEGIN(TestScript)
      TYPER_FIELD(floatValue)
      TYPER_FIELD(intValue)
      TYPER_FIELD(speed)
      TYPER_FIELD(doMove)
   TYPER_END(TestScript)
   TYPER_REGISTER_COMPONENT(TestScript);
   TYPER_REGISTER_NATIVE_SCRIPT(TestScript);

}
