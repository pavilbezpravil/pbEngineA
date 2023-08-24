#include "pch.h"
#include "PhysComponents.h"

#include "PhysUtils.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "gui/Gui.h"
#include "typer/Registration.h"
#include "typer/Serialize.h"


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

   JointComponent::JointComponent(JointType type) : type(type) { }

   void JointComponent::SetData(physx::PxPhysics* pxPhys) {
      // todo: ctor
      // todo: if type state the same it is not need to recreate joint
      if (pxJoint) {
         pxJoint->release();
         pxJoint = nullptr;
      }

      auto actor0 = GetPxActor(entity0);
      auto actor1 = GetPxActor(entity1);

      if (!actor0 || !actor1) {
         return;
      }

      if (!PxIsRigidDynamic(actor0) && !PxIsRigidDynamic(actor1)) {
         WARN("Joint can be created when min one actor is dynamic");
         return;
      }

      if (type == JointType::Fixed) {
         auto pxFixedJoint = PxFixedJointCreate(*pxPhys, actor0, PxTransform{ PxIDENTITY{} }, actor1, PxTransform{ PxIDENTITY{} });
         pxJoint = pxFixedJoint;
         if (!pxJoint) {
            WARN("Cant create fixed joint");
            return;
         }
      } else if (type == JointType::Distance) {
         auto pxDistanceJoint = PxDistanceJointCreate(*pxPhys, actor0, PxTransform{ PxIDENTITY{} }, actor1, PxTransform{ PxIDENTITY{} });
         pxJoint = pxDistanceJoint;
         if (!pxJoint) {
            WARN("Cant create distance joint");
            return;
         }

         pxDistanceJoint->setMinDistance(minDistance);
         pxDistanceJoint->setMaxDistance(maxDistance);

         pxDistanceJoint->setDamping(damping);
         pxDistanceJoint->setStiffness(stiffness);

         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, minDistance > 0);
         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, maxDistance > 0);
         pxDistanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eSPRING_ENABLED, stiffness > 0);
      } else if (type == JointType::Revolute) {
         auto pxRevoluteJoint = PxRevoluteJointCreate(*pxPhys, actor0, PxTransform{ PxIDENTITY{} }, actor1, PxTransform{ PxIDENTITY{} });
         pxJoint = pxRevoluteJoint;
         if (!pxJoint) {
            WARN("Cant create revolute joint");
            return;
         }
         // todo:
      } else if (type == JointType::Spherical) {
         auto pxSphericalJoint = PxSphericalJointCreate(*pxPhys, actor0, PxTransform{ PxIDENTITY{} }, actor1, PxTransform{ PxIDENTITY{} });
         pxJoint = pxSphericalJoint;
         if (!pxJoint) {
            WARN("Cant create spherical joint");
            return;
         }
         // todo:
      } else if (type == JointType::Prismatic) {
         auto pxPrismaticJoint = PxPrismaticJointCreate(*pxPhys, actor0, PxTransform{ PxIDENTITY{} }, actor1, PxTransform{ PxIDENTITY{} });
         pxJoint = pxPrismaticJoint;
         if (!pxJoint) {
            WARN("Cant create prismatic joint");
            return;
         }
         // todo:

         const auto tolerances = pxPhys->getTolerancesScale(); // todo: or set from component?

         // todo: name
         pxPrismaticJoint->setLimit(PxJointLinearLimitPair{ tolerances, minDistance, maxDistance });
         pxPrismaticJoint->setPrismaticJointFlag(PxPrismaticJointFlag::eLIMIT_ENABLED, true);
      } else {
         UNIMPLEMENTED();
      }

      pxJoint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, collisionEnable);
      pxJoint->setBreakForce(FloatInfToMax(breakForce), FloatInfToMax(breakTorque));

      WakeUp();
   }

   void JointComponent::WakeUp() {
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


#define TYPE_REGISTER_GUARD_UNIQUE(unique) \
   static TypeRegisterGuard CONCAT(TypeRegisterGuard_, unique)

#define TYPE_BEGIN(type) \
   TYPE_REGISTER_GUARD_UNIQUE(__LINE__) = \
      {GetTypeID<type>(), [] () { \
         using CurrentType = type; \
         TypeInfo ti; \
         ti.name = #type; \
         ti.typeID = GetTypeID<type>(); \
         ti.typeSizeOf = sizeof(type);

#define STRUCT_BEGIN(type) \
   TYPE_BEGIN(type) \
      ti.serialize = GetSerialize<type>(); \
      ti.deserialize = GetDeserialize<type>(); \
      ti.ui = GetUI<type>(); \
      \
      TypeField f{};

#define STRUCT_FIELD(_name) \
      f.name = #_name; \
      f.typeID = GetTypeID<decltype(CurrentType{}._name)>(); \
      f.offset = offsetof(CurrentType, _name); \
      ti.fields.emplace_back(f); \
      f = {};

#define STRUCT_END() \
      Typer::Get().RegisterType(ti.typeID, std::move(ti)); \
   }};

#define ENUM_BEGIN(type) \
   TYPE_BEGIN(type) \
      static std::string enumDescCombo;

#define ENUM_VALUE(Value) \
      CurrentType::Value; \
      enumDescCombo += STRINGIFY(Value); \
      enumDescCombo += '\0';

#define ENUM_END() \
      ti.serialize = [](Serializer& ser, const byte* value) { ser.out << *(int*)value; }; \
      ti.deserialize = [](const Deserializer& deser, byte* value) { *(int*)value = deser.node.as<int>(); return true; }; \
      ti.ui = [](const char* name, byte* value) { return ImGui::Combo(name, (int*)value, enumDescCombo.c_str()); }; \
      Typer::Get().RegisterType(ti.typeID, std::move(ti)); \
   }};


   ENUM_BEGIN(JointType)
      ENUM_VALUE(Fixed)
      ENUM_VALUE(Distance)
      ENUM_VALUE(Revolute)
      ENUM_VALUE(Spherical)
      ENUM_VALUE(Prismatic)
   ENUM_END()

   STRUCT_BEGIN(JointComponent::DistanceJoint)
      STRUCT_FIELD(minDistance)
      STRUCT_FIELD(maxDistance)
   STRUCT_END()

   STRUCT_BEGIN(JointComponent::PrismaticJoint)
      TYPER_FIELD(lower)
      TYPER_FIELD(upper)
   TYPER_END()
   
   STRUCT_BEGIN(JointComponent)
      TYPER_FIELD(type)
   
      TYPER_FIELD(entity0)
      TYPER_FIELD(entity1)
   
      TYPER_FIELD(distance)
      TYPER_FIELD(prismatic)
   
      TYPER_FIELD(minDistance)
      TYPER_FIELD(maxDistance)
   
      TYPER_FIELD(stiffness)
      TYPER_FIELD(damping)
   
      TYPER_FIELD(breakForce)
      TYPER_FIELD(breakTorque)
      TYPER_FIELD(collisionEnable)
   STRUCT_END()

   TYPER_REGISTER_COMPONENT(RigidBodyComponent);
   TYPER_REGISTER_COMPONENT(TriggerComponent);
   TYPER_REGISTER_COMPONENT(JointComponent);

}
