#include "pch.h"
#include "PhysComponents.h"

#include "Phys.h"
#include "PhysUtils.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "gui/Gui.h"
#include "typer/Registration.h"
#include "typer/Serialize.h"

#include <NvBlastTk.h>
#include <NvBlastExtDamageShaders.h>


namespace pbe {

   void RigidBodyComponent::SetLinearVelocity(const vec3& v, bool autowake) {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->setLinearVelocity(Vec3ToPx(v), autowake);
   }

   void RigidBodyComponent::SetData() {
      if (!dynamic) {
         return;
      }
      
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->setLinearDamping(linearDamping);
      dynamic->setAngularDamping(angularDamping);
   }

   void DestructComponent::ApplyDamageAtLocal(const vec3& posL, float damage) {
      // todo:
      static NvBlastExtMaterial material;
      material.health = 10.0f;
      material.minDamageThreshold = 0.0f;
      material.maxDamageThreshold = 1.0f;

      // todo:
      static NvBlastDamageProgram damageProgram = { NvBlastExtFalloffGraphShader, NvBlastExtFalloffSubgraphShader };
      damageProgram.graphShaderFunction = NvBlastExtFalloffGraphShader;
      damageProgram.subgraphShaderFunction = NvBlastExtFalloffSubgraphShader;

      static NvBlastExtRadialDamageDesc damageDesc = { damage, {posL.x, posL.y, posL.z}, 0.5f, 1.f };
      static NvBlastExtProgramParams params{ &damageDesc, &material, /*NvBlastExtDamageAccelerator*/ nullptr }; // todo: accelerator

      tkActor->damage(damageProgram, &params);
   }

   JointComponent::JointComponent(JointType type) : type(type) { }

   void JointComponent::SetData(const Entity& entity) {
      // todo: ctor
      // todo: if type state the same it is not need to recreate joint
      if (pxJoint) {
         // todo: several place for this
         delete (Entity*)pxJoint->userData;
         pxJoint->userData = nullptr;
         pxJoint->release();
         pxJoint = nullptr;
      }

      auto actor0 = GetPxActor(entity0);
      auto actor1 = GetPxActor(entity1);

      if (!actor0 || !actor1) {
         return;
      }

      auto localFrame0 = PxTransform{ Vec3ToPx(anchor0.position), QuatToPx(anchor0.rotation) };
      auto localFrame1 = PxTransform{ Vec3ToPx(anchor1.position), QuatToPx(anchor1.rotation) };

      if (!PxIsRigidDynamic(actor0) && !PxIsRigidDynamic(actor1)) {
         WARN("Joint: at least one actor must be non-static");
         return;
      }

      if (type == JointType::Fixed) {
         auto pxFixedJoint = PxFixedJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxFixedJoint;
         if (!pxJoint) {
            WARN("Cant create fixed joint");
            return;
         }
      } else if (type == JointType::Distance) {
         auto pxDistanceJoint = PxDistanceJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxDistanceJoint;
         if (!pxJoint) {
            WARN("Cant create distance joint");
            return;
         }

         pxDistanceJoint->setMinDistance(distance.minDistance);
         pxDistanceJoint->setMaxDistance(distance.maxDistance);

         pxDistanceJoint->setDamping(distance.damping);
         pxDistanceJoint->setStiffness(distance.stiffness);

         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, distance.minDistance > 0);
         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, distance.maxDistance > 0);
         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eSPRING_ENABLED, distance.stiffness > 0);
      } else if (type == JointType::Revolute) {
         auto pxRevoluteJoint = PxRevoluteJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxRevoluteJoint;
         if (!pxJoint) {
            WARN("Cant create revolute joint");
            return;
         }

         if (revolute.limitEnable) {
            pxRevoluteJoint->setLimit(PxJointAngularLimitPair{ revolute.lowerLimit, revolute.upperLimit,
               PxSpring{ revolute.stiffness, revolute.damping } });
            pxRevoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eLIMIT_ENABLED, revolute.limitEnable);
         }

         if (revolute.driveEnable) {
            pxRevoluteJoint->setDriveVelocity(revolute.driveVelocity);
            pxRevoluteJoint->setDriveForceLimit(FloatInfToMax(revolute.driveForceLimit));
            pxRevoluteJoint->setDriveGearRatio(revolute.driveGearRatio);

            pxRevoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_ENABLED, revolute.driveEnable);
            pxRevoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eDRIVE_FREESPIN, revolute.driveFreespin);
         }
      } else if (type == JointType::Spherical) {
         auto pxSphericalJoint = PxSphericalJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxSphericalJoint;
         if (!pxJoint) {
            WARN("Cant create spherical joint");
            return;
         }
         // todo:
      } else if (type == JointType::Prismatic) {
         auto pxPrismaticJoint = PxPrismaticJointCreate(*GetPxPhysics(), actor0, localFrame0, actor1, localFrame1);
         pxJoint = pxPrismaticJoint;
         if (!pxJoint) {
            WARN("Cant create prismatic joint");
            return;
         }
         // todo:

         const auto tolerances = GetPxPhysics()->getTolerancesScale(); // todo: or set from component?

         // todo: name
         pxPrismaticJoint->setLimit(PxJointLinearLimitPair{ tolerances, prismatic.lowerLimit, prismatic.upperLimit });
         pxPrismaticJoint->setPrismaticJointFlag(PxPrismaticJointFlag::eLIMIT_ENABLED, true);
      } else {
         UNIMPLEMENTED();
      }

      pxJoint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, collisionEnable);
      pxJoint->setBreakForce(FloatInfToMax(breakForce), FloatInfToMax(breakTorque));

      auto pEntity = new Entity{ entity }; // todo: allocation
      pxJoint->userData = pEntity;
      pxJoint->getConstraint()->userData = pEntity;

      WakeUp();
   }

   void JointComponent::WakeUp() {
      auto actor0 = GetPxActor(entity0);
      auto actor1 = GetPxActor(entity1);

      PxWakeUp(actor0);
      PxWakeUp(actor1);
   }

   std::optional<Transform> JointComponent::GetAnchorTransform(Anchor anchor) const {
      auto actor = GetPxActor(anchor == Anchor::Anchor0 ? entity0 : entity1);
      if (!actor) {
         return {};
      }
      auto localFrame = anchor == Anchor::Anchor0 ? anchor0 : anchor1;
      // todo: use pb math instead of px
      auto globalFrame = actor->getGlobalPose() * PxTransform{ Vec3ToPx(localFrame.position), QuatToPx(localFrame.rotation) };
      return std::optional{ Transform{PxVec3ToPBE(globalFrame.p), PxQuatToPBE(globalFrame.q)} };
   }


   STRUCT_BEGIN(RigidBodyComponent)
      STRUCT_FIELD(dynamic)
      STRUCT_FIELD(linearDamping)
      STRUCT_FIELD(angularDamping)
   STRUCT_END()

   STRUCT_BEGIN(RigidBodyShapeComponent)
   STRUCT_END()

   STRUCT_BEGIN(TriggerComponent)
   STRUCT_END()

   STRUCT_BEGIN(DestructComponent)
   STRUCT_END()

   ENUM_BEGIN(JointType)
      ENUM_VALUE(Fixed)
      ENUM_VALUE(Distance)
      ENUM_VALUE(Revolute)
      ENUM_VALUE(Spherical)
      ENUM_VALUE(Prismatic)
   ENUM_END()

   STRUCT_BEGIN(JointAnchor)
      STRUCT_FIELD(position)
      STRUCT_FIELD(rotation)
   STRUCT_END()

   STRUCT_BEGIN(JointComponent::DistanceJoint)
      STRUCT_FIELD(minDistance)
      STRUCT_FIELD(maxDistance)

      STRUCT_FIELD(stiffness)
      STRUCT_FIELD(damping)
   STRUCT_END()

   STRUCT_BEGIN(JointComponent::RevoluteJoint)
      STRUCT_FIELD(limitEnable)
      STRUCT_FIELD(lowerLimit)
      STRUCT_FIELD(upperLimit)
      STRUCT_FIELD(stiffness)
      STRUCT_FIELD(damping)

      STRUCT_FIELD(driveEnable)
      STRUCT_FIELD(driveFreespin)
      STRUCT_FIELD(driveVelocity)
      STRUCT_FIELD(driveForceLimit)
      STRUCT_FIELD(driveGearRatio)
   STRUCT_END()

   STRUCT_BEGIN(JointComponent::PrismaticJoint)
      STRUCT_FIELD(lowerLimit)
      STRUCT_FIELD(upperLimit)
   STRUCT_END()

   auto CheckJointType(JointType type) {
      return [=](auto* byte) { return ((JointComponent*)byte)->type == type; };
   }

   STRUCT_BEGIN(JointComponent)
      STRUCT_FIELD(type)
   
      STRUCT_FIELD(entity0)
      STRUCT_FIELD(entity1)

      STRUCT_FIELD(anchor0)
      STRUCT_FIELD(anchor1)

      STRUCT_FIELD_USE(CheckJointType(JointType::Distance))
      // STRUCT_FIELD_FLAG(SkipName)
      STRUCT_FIELD(distance)

      STRUCT_FIELD_USE(CheckJointType(JointType::Revolute))
      // STRUCT_FIELD_FLAG(SkipName)
      STRUCT_FIELD(revolute)

      STRUCT_FIELD_USE(CheckJointType(JointType::Prismatic))
      // STRUCT_FIELD_FLAG(SkipName)
      STRUCT_FIELD(prismatic)

      STRUCT_FIELD(breakForce)
      STRUCT_FIELD(breakTorque)
      STRUCT_FIELD(collisionEnable)
   STRUCT_END()

   TYPER_REGISTER_COMPONENT(RigidBodyComponent);
   TYPER_REGISTER_COMPONENT(RigidBodyShapeComponent);
   TYPER_REGISTER_COMPONENT(DestructComponent);
   TYPER_REGISTER_COMPONENT(TriggerComponent);
   TYPER_REGISTER_COMPONENT(JointComponent);

}
