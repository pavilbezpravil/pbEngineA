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

   enum class JointType {
      Fixed,
      Distance,
      Revolute,
      Spherical,
      Prismatic,
      // D6,
      // Gear,
      // RackAndPinion,
      // Contact
   };

   struct JointComponent {
      JointType type = JointType::Distance;

      Entity entity0;
      Entity entity1;

      // vec3 anchor0;
      // vec3 anchor1;

      struct FixedJoint {
      } fixed;

      struct DistanceJoint {
         float minDistance = 0;
         float maxDistance = 0;

         float stiffness = 1000.f;
         float damping = 0.5;
      } distance;

      struct RevoluteJoint {
         // limit
         bool limitEnable = false; // todo: make flags
         float lowerLimit = 0;
         float upperLimit = 0;
         float stiffness = 0;
         float damping = 0;

         bool driveEnable = false; // todo: make flags
         bool driveFreespin = false; // todo: make flags
         float driveVelocity = 0;
         float driveForceLimit = INFINITY;
         float driveGearRatio = 1;
      } revolute;

      struct SphericalJoint {
      } spherical;

      struct PrismaticJoint {
         float lowerLimit = -1;
         float upperLimit = 1;
      } prismatic;

      float breakForce = INFINITY;
      float breakTorque = INFINITY;
      bool collisionEnable = false;

      physx::PxJoint* pxJoint = nullptr;

      JointComponent() = default;
      JointComponent(JointType type);

      void SetData(physx::PxPhysics* pxPhys);

      void WakeUp();
   };

}
