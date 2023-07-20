#pragma once


namespace pbe {
   class Entity;
   class Scene;

   using namespace physx;

   void InitPhysics();
   void TermPhysics();

   class PhysicsScene : public PxSimulationEventCallback {
   public:
      PhysicsScene(Scene& scene);
      ~PhysicsScene() override;

      void SyncPhysicsWithScene();
      void Simulation(float dt);
      void UpdateSceneAfterPhysics();

      void AddRigidActor(Entity entity);
      void RemoveRigidActor(Entity entity);

      void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override {}
      void onWake(PxActor** actors, PxU32 count) override {}
      void onSleep(PxActor** actors, PxU32 count) override {}
      void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override {}
      void onTrigger(PxTriggerPair* pairs, PxU32 count) override {}
      void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override {}
   private:
      PxScene* pxScene = nullptr;
      Scene& scene;

      float timeAccumulator = 0;

      friend class Scene;
      void OnConstructRigidBody(entt::registry& registry, entt::entity entity);
      void OnDestroyRigidBody(entt::registry& registry, entt::entity entity);

      void OnConstructDistanceJoint(entt::registry& registry, entt::entity entity);
      void OnDestroyDistanceJoint(entt::registry& registry, entt::entity entity);
   };

}
