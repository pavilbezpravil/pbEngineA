#pragma once
#include "core/Core.h"
#include "scene/System.h"
#include "utils/TimedAction.h"
#include "math/Types.h"


namespace pbe {
   class Entity;
   class Scene;

   using namespace physx;

   void InitPhysics();
   void TermPhysics();

   struct RayCastResult;

   class CORE_API PhysicsScene : public System {
   public:
      PhysicsScene(Scene& scene);
      ~PhysicsScene() override;

      RayCastResult RayCast(const vec3& origin, const vec3& dir, float maxDistance);

      void SyncPhysicsWithScene();
      void Simulate(float dt);
      void UpdateSceneAfterPhysics();

      void OnSetEventHandlers(entt::registry& registry) override;
      void OnEntityEnable() override;
      void OnEntityDisable() override;

      void OnUpdate(float dt) override;

   private:
      PxScene* pxScene = nullptr;
      Scene& scene;

      TimedAction stepTimer{60.f};

      void AddRigidActor(Entity entity);
      void RemoveRigidActor(Entity entity);

      void AddTrigger(Entity entity);
      void RemoveTrigger(Entity entity);

      void AddDistanceJoint(Entity entity);
      void RemoveDistanceJoint(Entity entity);

      friend class Scene;
      void OnConstructRigidBody(entt::registry& registry, entt::entity entity);
      void OnDestroyRigidBody(entt::registry& registry, entt::entity entity);

      void OnConstructTrigger(entt::registry& registry, entt::entity entity);
      void OnDestroyTrigger(entt::registry& registry, entt::entity entity);

      void OnConstructDistanceJoint(entt::registry& registry, entt::entity entity);
      void OnDestroyDistanceJoint(entt::registry& registry, entt::entity entity);
      void OnUpdateDistanceJoint(entt::registry& registry, entt::entity entity);
   };

   // todo:
   CORE_API void CreateDistanceJoint(const Entity& entity0, const Entity& entity1);

}
