#pragma once
#include "core/Core.h"
#include "math/Transform.h"
#include "scene/Entity.h"


namespace Nv {
   namespace Blast {
      class TkActor;
      class TkAsset;
   }
}

namespace pbe { 

   // todo: while have two way for handle change in component data
   // in future choose one

   struct CORE_API RigidBodyComponent {
      // todo: mb have different components for static and dynamic rigid bodies
      bool dynamic = false;

      float linearDamping = 0.1f;
      float angularDamping = 0.1f;

      physx::PxRigidActor* pxRigidActor = nullptr;

      void SetLinearVelocity(const vec3& v, bool autowake = true);

      void SetData();
   };

   struct CORE_API RigidBodyShapeComponent {
      // todo:
      float friction = 0.7f;
   };

   // todo: name
   struct DestructData {
      Nv::Blast::TkAsset* tkAsset = nullptr;
      std::vector<vec3> chunkSizes; // todo:
   };

   // todo: name
   struct CORE_API DestructComponent {
      float hardness = 1.f; // todo:

      // todo:
      void ApplyDamageAtLocal(const vec3& posL, float damage);

      bool root = true; // todo:
      bool releaseTkActor = true; // todo:

      DestructData* destructData = nullptr;

      Nv::Blast::TkActor* tkActor = nullptr;
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

   struct JointAnchor {
      vec3 position{};
      quat rotation = quat_Identity;
   };

   struct JointComponent {
      JointType type = JointType::Distance;

      Entity entity0;
      Entity entity1;

      JointAnchor anchor0;
      JointAnchor anchor1;

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

      bool IsValid() const { return pxJoint != nullptr && entity0 && entity1; }

      void SetData(const Entity& entity);

      void WakeUp();

      enum class Anchor {
         Anchor0,
         Anchor1,
      };

      std::optional<Transform> GetAnchorTransform(Anchor anchor) const;
   };

}
