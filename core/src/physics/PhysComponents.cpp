#include "pch.h"
#include "PhysComponents.h"

#include "PhysUtils.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "typer/Registration.h"


namespace pbe {

   void RigidBodyComponent::SetLinearVelocity(const vec3& v, bool autowake) {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->setLinearVelocity(Vec3ToPx(v), autowake);
   }

   void RigidBodyComponent::OnChanged() {
      // todo: dont support changing dynamic/static
      if (!dynamic) {
         return;
      }

      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->setLinearDamping(linearDamping);
      dynamic->setAngularDamping(angularDamping);
   }

   void DistanceJointComponent::SetData() {
      auto joint = pxDistanceJoint;
      if (!joint) {
         return;
      }

      joint->setBreakForce(FloatInfToMax(breakForce), FloatInfToMax(breakTorque));

      joint->setMinDistance(minDistance);
      joint->setMaxDistance(maxDistance);

      joint->setDamping(damping);
      joint->setStiffness(stiffness);

      PxDistanceJointFlags flags = (PxDistanceJointFlags)0;
      if (minDistance > 0) {
         flags |= PxDistanceJointFlag::eMIN_DISTANCE_ENABLED;
      }
      if (maxDistance > 0) {
         flags |= PxDistanceJointFlag::eMAX_DISTANCE_ENABLED;
      }
      if (stiffness > 0) {
         flags |= PxDistanceJointFlag::eSPRING_ENABLED;
      }
      joint->setDistanceJointFlags(flags);

      WakeUp();
   }

   void DistanceJointComponent::WakeUp() {
      auto actor0 = GetPxActor(entity0);
      auto actor1 = GetPxActor(entity1);

      PxWakeUp(actor0);
      PxWakeUp(actor1);
   }


   TYPER_BEGIN(RigidBodyComponent)
      TYPER_FIELD(dynamic)
      TYPER_FIELD(linearDamping)
      TYPER_FIELD(angularDamping)
   TYPER_END()

   TYPER_BEGIN(TriggerComponent)
   TYPER_END()

   TYPER_BEGIN(DistanceJointComponent)
      TYPER_FIELD(entity0)
      TYPER_FIELD(entity1)

      TYPER_FIELD(minDistance)
      TYPER_FIELD(maxDistance)

      TYPER_FIELD(stiffness)
      TYPER_FIELD(damping)

      TYPER_FIELD(breakForce)
      TYPER_FIELD(breakTorque)
   TYPER_END()

   TYPER_REGISTER_COMPONENT(RigidBodyComponent);
   TYPER_REGISTER_COMPONENT(TriggerComponent);
   TYPER_REGISTER_COMPONENT(DistanceJointComponent);

}
