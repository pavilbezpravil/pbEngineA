#include "pch.h"
#include "PhysUtils.h"

#include "PhysComponents.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "scene/Component.h"
#include "scene/Entity.h"


namespace pbe {

   Entity* GetEntity(PxActor* actor) {
      return (Entity*)actor->userData;
   }

   PxTransform GetTransform(const SceneTransformComponent& trans) {
      return PxTransform{ Vec3ToPx(trans.Position()), QuatToPx(trans.Rotation()) };
   }

   PxGeometryHolder GetPhysGeom(const SceneTransformComponent& trans, const GeometryComponent& geom) {
      // todo: think about tran.scale
      PxGeometryHolder physGeom;
      if (geom.type == GeomType::Sphere) {
         physGeom = PxSphereGeometry(geom.sizeData.x * trans.Scale().x); // todo: scale, default radius == 1 -> diameter == 2 (it is bad)
      } else if (geom.type == GeomType::Box) {
         physGeom = PxBoxGeometry(Vec3ToPx(geom.sizeData * trans.Scale() / 2.f));
      }
      return physGeom;
   }

   PxRigidActor* GetPxActor(Entity e) {
      if (!e) return nullptr;

      auto rb = e.TryGet<RigidBodyComponent>();
      return rb ? rb->GetPxRigidActor() : nullptr;
   }

   PxRigidDynamic* GetPxRigidDynamic(PxRigidActor* actor) {
      return actor->is<PxRigidDynamic>();
   }

   bool PxIsRigidDynamic(PxRigidActor* actor) {
      return actor->is<PxRigidDynamic>() != nullptr;
   }

   void PxWakeUp(PxRigidActor* actor) {
      if (!actor) {
         return;
      }

      PxRigidDynamic* dynActor = GetPxRigidDynamic(actor);
      if (dynActor && dynActor->isSleeping()) {
         dynActor->wakeUp();
      }
   }

   float FloatInfToMax(float v) {
      return v == INFINITY ? PX_MAX_F32 : v;
   }

}
