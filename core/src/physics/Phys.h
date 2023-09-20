#pragma once


namespace pbe {

   using namespace physx;

   class PhysicsScene;
   class Entity;
   class Scene;

   void InitPhysics();
   void TermPhysics();

   PxPhysics* GetPxPhysics();
   PxCpuDispatcher* GetPxCpuDispatcher();
   PxMaterial* GetPxMaterial();

   PxFilterFlags PxSimulationFilterShader(
      PxFilterObjectAttributes attributes0, PxFilterData filterData0,
      PxFilterObjectAttributes attributes1, PxFilterData filterData1,
      PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize);

   struct SimulationEventCallback : PxSimulationEventCallback {
      PhysicsScene* physScene{};
      SimulationEventCallback(PhysicsScene* physScene);

      void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override;
      void onWake(PxActor** actors, PxU32 count) override;
      void onSleep(PxActor** actors, PxU32 count) override;
      void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override;
      void onTrigger(PxTriggerPair* pairs, PxU32 count) override;
      void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override;
   };

}
