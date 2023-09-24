#include "pch.h"

#include "math/Random.h"
#include "physics/PhysComponents.h"
#include "scene/Component.h"
#include "scene/Utils.h"
#include "script/Script.h"
#include "typer/Registration.h"
#include "utils/TimedAction.h"


namespace pbe {

   struct TestValues {
      float floatValue = 2;
      int intValue = 2;
   };

   STRUCT_BEGIN(TestValues)
      STRUCT_FIELD(floatValue)
      STRUCT_FIELD(intValue)
   STRUCT_END()

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
            tran.Local().position.y += speed * dt;
         }
      }

      TestValues values;
      float speed = 1;
      bool doMove = false;
   };

   STRUCT_BEGIN(TestScript)
      STRUCT_FIELD(values)
      STRUCT_FIELD(speed)
      STRUCT_FIELD(doMove)
   STRUCT_END()
   TYPER_REGISTER_COMPONENT(TestScript);
   TYPER_REGISTER_SCRIPT(TestScript);


   class CubeSpawnerScript : public Script {
   public:
      void OnEnable() override {
         timer = {freq};
         INFO(__FUNCTION__);
      }

      void OnDisable() override {
         INFO(__FUNCTION__);
      }

      void OnUpdate(float dt) override {
         if (spawn) {
            auto& tran = owner.GetTransform();

            int steps = timer.Update(dt);
            while (steps-- > 0) {
               Entity e = CreateCube(GetScene(), CubeDesc {
                     .parent = owner,
                     .color = Random::Color(),
                  });

               e.Get<RigidBodyComponent>().SetLinearVelocity(tran.Forward() * initialSpeed);
            }
         } else {
            timer.Reset();
         }
      }

      bool spawn = false;
      float freq = 1;
      float initialSpeed = 0;

   private:
      TimedAction timer;
   };

   STRUCT_BEGIN(CubeSpawnerScript)
      STRUCT_FIELD(spawn)
      STRUCT_FIELD(freq)
      STRUCT_FIELD(initialSpeed)
   STRUCT_END()
   TYPER_REGISTER_COMPONENT(CubeSpawnerScript);
   TYPER_REGISTER_SCRIPT(CubeSpawnerScript);

}
