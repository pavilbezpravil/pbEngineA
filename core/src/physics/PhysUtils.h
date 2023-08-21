#pragma once


namespace pbe {

   struct GeometryComponent;
   struct SceneTransformComponent;
   class Entity;

   Entity* GetEntity(physx::PxActor* actor);

   physx::PxTransform GetTransform(const SceneTransformComponent& trans);
   physx::PxGeometryHolder GetPhysGeom(const SceneTransformComponent& trans, const GeometryComponent& geom);

   physx::PxRigidActor* GetPxActor(Entity e);
   physx::PxRigidDynamic* GetPxRigidDynamic(physx::PxRigidActor* actor);

   bool PxIsRigidDynamic(physx::PxRigidActor* actor);

   void PxWakeUp(physx::PxRigidActor* actor);

   float FloatInfToMax(float v);

}
