#pragma once
#include "core/Core.h"
#include "scene/Entity.h"


namespace pbe { 

   // todo: while have two way for handle change in component data
   // in future choose one

   struct CORE_API RigidBodyComponent {
      // todo: mb have different components for static and dynamic rigid bodies
      bool dynamic = false;

      float linearDamping = 0.5f;
      float angularDamping = 0.5f;

      physx::PxRigidActor* pxRigidActor = nullptr;

      void SetData();

      void SetLinearVelocity(const vec3& v, bool autowake = true);

      void OnChanged();
   };

   struct TriggerComponent {
      physx::PxRigidActor* pxRigidActor = nullptr;
   };

   struct DistanceJointComponent {
      Entity entity0;
      Entity entity1;

      float minDistance = 0;
      float maxDistance = 0;

      float stiffness = 1000.f;
      float damping = 0.5;

      float breakForce = INFINITY;
      float breakTorque = INFINITY;

      physx::PxDistanceJoint* pxDistanceJoint = nullptr;

      void SetData();

      void WakeUp();
   };


}
