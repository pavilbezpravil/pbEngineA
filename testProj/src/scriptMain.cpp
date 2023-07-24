#include "pch.h"

#include "math/Random.h"
#include "scene/Component.h"
#include "scene/Utils.h"
#include "script/Script.h"
#include "typer/Typer.h"
#include "utils/TimedAction.h"


namespace pbe {

   struct TestValues {
      float floatValue = 2;
      int intValue = 2;
   };

   TYPER_BEGIN(TestValues)
      TYPER_FIELD(floatValue)
      TYPER_FIELD(intValue)
   TYPER_END()

   class TestScript : public Script {
   public:
      void OnEnable() override {
         INFO("OnEnable {}", GetName());
      }

      void OnDisable() override {
         INFO("OnDisable {}", GetName());
      }

      void OnUpdate(float dt) override {
         if (doMove) {
            auto& tran= owner.Get<SceneTransformComponent>();
            tran.position.y += speed * dt;
         }
      }

      TestValues values;
      float speed = 1;
      bool doMove = false;
   };

   TYPER_BEGIN(TestScript)
      TYPER_FIELD(values)
      TYPER_FIELD(speed)
      TYPER_FIELD(doMove)
   TYPER_END()
   TYPER_REGISTER_COMPONENT(TestScript);
   TYPER_REGISTER_SCRIPT(TestScript);


   class CubeSpawnerScript : public Script {
   public:
      void OnEnable() override {
         timer = {freq};
      }

      void OnDisable() override {
         
      }

      void OnUpdate(float dt) override {
         if (spawn) {
            auto& tran = owner.GetTransform();

            int steps = timer.Update(dt);
            while (steps-- > 0) {
               Entity e = CreateCube(GetScene(), CubeDesc{
               .parent = owner,
               .color = Random::Color()
                  });

               e.Get<RigidBodyComponent>().SetLinearVelocity(tran.Forward() * initialSpeed);
            }
         }
      }

      float initialSpeed = 0;
      float freq = 1;
      bool spawn = false;

   private:
      TimedAction timer;
   };

   TYPER_BEGIN(CubeSpawnerScript)
      TYPER_FIELD(spawn)
      TYPER_FIELD(freq)
      TYPER_FIELD(initialSpeed)
   TYPER_END()
   TYPER_REGISTER_COMPONENT(CubeSpawnerScript);
   TYPER_REGISTER_SCRIPT(CubeSpawnerScript);

}
