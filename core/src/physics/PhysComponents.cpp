#include "pch.h"
#include "PhysComponents.h"

#include "Phys.h"
#include "PhysUtils.h"
#include "PhysXTypeConvet.h"
#include "DestructEventListener.h"
#include "core/Profiler.h"
#include "gui/Gui.h"
#include "typer/Registration.h"
#include "typer/Serialize.h"
#include "scene/Component.h"

#include <NvBlastTk.h>
#include <NvBlastExtDamageShaders.h>

#include "PhysicsScene.h"
#include "math/Color.h"
#include "math/Shape.h"
#include "rend/DbgRend.h"


using namespace Nv::Blast;

namespace pbe {

   static void Slice(std::vector<NvBlastChunkDesc>& chunkDescs,
      std::vector<vec3>& chunkSizes,
      std::vector<NvBlastBondDesc>& bondDescs,
      uint parentChunkIdx,
      NvBlastChunkDesc::Flags flags) {

      vec3 chunkCenter = {
         chunkDescs[parentChunkIdx].centroid[0],
         chunkDescs[parentChunkIdx].centroid[1],
         chunkDescs[parentChunkIdx].centroid[2],
      };
      vec3 chunkSize = chunkSizes[parentChunkIdx];

      uint slices = std::max(1, (int)chunkSize.x);

      uint chunkIdx = (uint)chunkDescs.size();
      uint boundIdx = (uint)bondDescs.size();

      chunkDescs.resize(chunkIdx + slices + 1);
      chunkSizes.resize(chunkIdx + slices + 1);

      if (flags & NvBlastChunkDesc::SupportFlag) {
         bondDescs.resize(boundIdx + slices);
      }

      float chunkParentVolume = chunkSize.x * chunkSize.y * chunkSize.z;
      float chunkParentSliceAxisSize = chunkSize.x;
      float sliceAxisStart = -chunkSize.x * 0.5f;
      float chunkSliceAxisSize = chunkParentSliceAxisSize / (slices + 1);
      float chunkVolume = chunkParentVolume / (slices + 1);

      for (uint i = 0; i <= slices; ++i) {
         auto& chunkDesc = chunkDescs[chunkIdx];
         chunkDesc.parentChunkDescIndex = parentChunkIdx;
         chunkDesc.centroid[0] = chunkCenter[0] + sliceAxisStart + chunkSliceAxisSize * (0.5f + i);
         chunkDesc.centroid[1] = chunkCenter[1];
         chunkDesc.centroid[2] = chunkCenter[2];
         chunkDesc.volume = chunkVolume;
         // chunkDesc.flags = NvBlastChunkDesc::SupportFlag;
         // chunkDesc.flags = NvBlastChunkDesc::NoFlags;
         chunkDesc.flags = flags;
         chunkDesc.userData = chunkIdx;

         chunkSizes[chunkIdx] = vec3{ chunkSliceAxisSize, chunkSize.y, chunkSize.z };

         ++chunkIdx;

         if (i < slices && flags & NvBlastChunkDesc::SupportFlag) {
            auto& boundDesc = bondDescs[boundIdx++];

            boundDesc.chunkIndices[0] = chunkIdx - 1; // chunkIndices refer to chunk descriptor indices for support chunks
            boundDesc.chunkIndices[1] = chunkIdx;
            boundDesc.bond.normal[0] = 1.0f;
            boundDesc.bond.normal[1] = 0.0f;
            boundDesc.bond.normal[2] = 0.0f;
            boundDesc.bond.area = chunkSize.y * chunkSize.z; // todo:
            boundDesc.bond.centroid[0] = chunkCenter[0] + sliceAxisStart + chunkSliceAxisSize * (i + 1); // todo:
            boundDesc.bond.centroid[1] = chunkCenter[1];
            boundDesc.bond.centroid[2] = chunkCenter[2];
         }
      }
   }

   static DestructData* GetDestructData(const vec3& chunkSize) {
      // uint slices = std::max(1, (int)chunkSize.x);

      std::vector<NvBlastChunkDesc> chunkDescs;
      std::vector<NvBlastBondDesc> bondDescs;
      std::vector<vec3> chunkSizes;

      chunkDescs.resize(1);
      chunkSizes.resize(1);

      // parent
      chunkDescs[0].parentChunkDescIndex = UINT32_MAX; // invalid index denotes a chunk hierarchy root
      chunkDescs[0].centroid[0] = 0.0f;
      chunkDescs[0].centroid[1] = 0.0f;
      chunkDescs[0].centroid[2] = 0.0f;
      chunkDescs[0].volume = 1.0f;
      chunkDescs[0].flags = NvBlastChunkDesc::NoFlags;
      chunkDescs[0].userData = 0;

      chunkSizes[0] = chunkSize;

      uint startChunkIdx = (uint)chunkDescs.size();
      Slice(chunkDescs, chunkSizes, bondDescs, 0, NvBlastChunkDesc::SupportFlag);

      // Slice(chunkDescs, chunkSizes, bondDescs, 1, NvBlastChunkDesc::NoFlags);
      if (1) {
         uint startChunkIdx2 = (uint)chunkDescs.size();

         uint nNewChunks = (uint)chunkDescs.size() - startChunkIdx;
         for (uint i = 0; i < nNewChunks; ++i) {
            Slice(chunkDescs, chunkSizes, bondDescs, startChunkIdx + i, NvBlastChunkDesc::NoFlags);
         }

         uint nNewChunks2 = (uint)chunkDescs.size() - startChunkIdx2;
         for (uint i = 0; i < nNewChunks2; ++i) {
            Slice(chunkDescs, chunkSizes, bondDescs, startChunkIdx2 + i, NvBlastChunkDesc::NoFlags);
         }
      }

      // bondDescs[0].userData = 0;  // this can be used to tell the user more information about this
      // bond for example to create a joint when this bond breaks

      // bondDescs[1].chunkIndices[0] = 1;
      // bondDescs[1].chunkIndices[1] = ~0;  // ~0 (UINT32_MAX) is the "invalid index."  This creates a world bond
      // ... etc. for bondDescs[1], all other fields are filled in as usual

      TkAssetDesc assetDesc;
      assetDesc.chunkCount = (uint)chunkDescs.size();
      assetDesc.chunkDescs = chunkDescs.data();
      assetDesc.bondCount = (uint)bondDescs.size();
      assetDesc.bondDescs = bondDescs.data();
      assetDesc.bondFlags = nullptr;

      auto tkFramework = NvBlastTkFrameworkGet();

      bool wasCoverage = tkFramework->ensureAssetExactSupportCoverage(chunkDescs.data(), assetDesc.chunkCount);
      INFO("Was coverage {}", wasCoverage);

      // todo: 'chunkReorderMap' may be skipped
      std::vector<uint32_t> chunkReorderMap(chunkDescs.size());  // Will be filled with a map from the original chunk descriptor order to the new one
      bool requireReordering = !tkFramework->reorderAssetDescChunks(chunkDescs.data(), assetDesc.chunkCount, bondDescs.data(), assetDesc.bondCount, chunkReorderMap.data());
      INFO("Require reordering {}", requireReordering);

      TkAsset* tkAsset = tkFramework->createAsset(assetDesc);

      // todo:
      DestructData* destructData = new DestructData{};
      destructData->tkAsset = tkAsset;
      destructData->chunkSizes = std::move(chunkSizes);
      return destructData;
   }

   void RigidBodyComponent::ApplyDamage(const vec3& posW, float damage) {
      if (!tkActor) {
         return;
      }

      auto posL = PxVec3ToPBE(pxRigidActor->getGlobalPose().transformInv(Vec3ToPx(posW)));

      // todo:
      float hardness = 500.f; // todo:
      static NvBlastExtMaterial material;
      material.health = hardness;
      material.minDamageThreshold = 0.2f;
      material.maxDamageThreshold = 1.0f;

      float normalizedDamage = material.getNormalizedDamage(damage);

      if (normalizedDamage == 0.f) {
         return;
      }

      INFO("Normalized damage: {}", normalizedDamage);

      static NvBlastDamageProgram damageProgram = { NvBlastExtFalloffGraphShader, NvBlastExtFalloffSubgraphShader };

      auto& damageDesc = GetPhysScene().GetDamageParamsPlace<NvBlastExtRadialDamageDesc>();
      auto& params = GetPhysScene().GetDamageParamsPlace<NvBlastExtProgramParams>();

      damageDesc = { normalizedDamage, {posL.x, posL.y, posL.z}, 1.0f, 2.f };
      params = { &damageDesc, nullptr, /*NvBlastExtDamageAccelerator*/ nullptr }; // todo: accelerator

      tkActor->damage(damageProgram, &params);
   }

   void RigidBodyComponent::SetLinearVelocity(const vec3& v, bool autowake) {
      ASSERT(dynamic);
      auto dynamic = GetPxRigidDynamic(pxRigidActor);
      dynamic->setLinearVelocity(Vec3ToPx(v), autowake);
   }

   void RigidBodyComponent::SetDestructible(Nv::Blast::TkActor& tkActor, DestructData& destructData) {
      ASSERT(!destructible);

      destructible = true;
      this->tkActor = &tkActor;
      this->destructData = &destructData;
      this->tkActor->userData = new Entity{ *(Entity*)pxRigidActor->userData }; // todo: use fixed allocator
   }

   void RigidBodyComponent::DbgRender(DbgRend& dbgRend) const {
      if (!tkActor) {
         return;
      }

      auto trans = GetEntity().GetTransform();

      auto tkAsset = tkActor->getAsset();
      auto tkChunks = tkAsset->getChunks();

      if (1) {
         std::array<uint, 32> visibleChunkIndices;

         uint32_t visibleChunkCount = tkActor->getVisibleChunkCount();
         ASSERT(visibleChunkCount < visibleChunkIndices.size());
         tkActor->getVisibleChunkIndices(visibleChunkIndices.data(), visibleChunkCount);

         for (uint iChunk = 0; iChunk < visibleChunkCount; ++iChunk) {
            auto chunkIndex = visibleChunkIndices[iChunk];
            NvBlastChunk chunk = tkChunks[chunkIndex];

            vec3 chunkCentroidL = vec3{ chunk.centroid[0], chunk.centroid[1], chunk.centroid[2] };
            vec3 chunkCentroidW = trans.World().TransformPosition(chunkCentroidL);
            // destructData->chunkSizes[chunkIndex]

            dbgRend.DrawSphere(Sphere{ chunkCentroidW, 0.1f }, Color_White, false);
         }
      }

      if (0) {
         for (uint iChunk = 0; iChunk < tkAsset->getChunkCount(); ++iChunk) {
            NvBlastChunk chunk = tkChunks[iChunk];

            vec3 chunkCentroidL = vec3{ chunk.centroid[0], chunk.centroid[1], chunk.centroid[2] };
            vec3 chunkCentroidW = trans.World().TransformPosition(chunkCentroidL);

            dbgRend.DrawSphere(Sphere{ chunkCentroidW, 0.1f }, Color_White, false);
         }
      }

      // for (uint i = 0; i < tkActor->getAsset()->getBondCount(); ++i) {
      //    tkActor->getBondHealths();
      //    tkActor->getAsset()->getBonds()[i].;
      // }
   }

   static void RemoveSceneRigidActor(PxScene& pxScene, PxRigidActor& pxRigidActor) {
      pxScene.removeActor(pxRigidActor);
      delete (Entity*)pxRigidActor.userData;
      pxRigidActor.userData = nullptr;
      pxRigidActor.release();
   }

   void RigidBodyComponent::CreateOrUpdate(PxScene& pxScene, Entity& entity) {
      if (!pxRigidActor || (bool)pxRigidActor->is<PxRigidDynamic>() != dynamic) {
         auto& trans = entity.Get<SceneTransformComponent>();
         PxTransform physTrans = GetTransform(trans);

         auto oldPxRigidActor = pxRigidActor;
         if (dynamic) {
            pxRigidActor = GetPxPhysics()->createRigidDynamic(physTrans);
         } else {
            pxRigidActor = GetPxPhysics()->createRigidStatic(physTrans);
         }
         pxRigidActor->userData = new Entity{ entity }; // todo: use fixed allocator

         // todo: tmp, remove after reconvert all scenes
         if (entity.Has<GeometryComponent>()) {
            entity.AddOrReplace<RigidBodyShapeComponent>();
         }

         AddShapesHier(entity);

         if (dynamic) {
            float density = 10.f; // todo:
            PxRigidBodyExt::updateMassAndInertia(*GetPxRigidDynamic(pxRigidActor), density);
         }

         pxScene.addActor(*pxRigidActor);

         if (oldPxRigidActor) {
            PxU32 nbConstrains = oldPxRigidActor->getNbConstraints();

            if (nbConstrains > 0) {
               std::array<PxConstraint*, 8> constrains;
               ASSERT(nbConstrains <= constrains.size());
               nbConstrains = std::min(nbConstrains, (uint)constrains.size());
               oldPxRigidActor->getConstraints(constrains.data(), nbConstrains);

               for (PxU32 i = 0; i < nbConstrains; i++) {
                  auto pxConstrain = constrains[i];
                  auto pEntity = (Entity*)pxConstrain->userData;
                  // todo: mb PxConstraint::userData should be PxJoint?
                  auto pxJoint = pEntity->Get<JointComponent>().pxJoint;

                  PxRigidActor *actor0, *actor1;
                  pxJoint->getActors(actor0, actor1);

                  if (actor0 == oldPxRigidActor) {
                     pxJoint->setActors(pxRigidActor, actor1);
                  } else {
                     ASSERT(actor1 == oldPxRigidActor);
                     pxJoint->setActors(actor0, pxRigidActor);
                  }
               }
            }

            RemoveSceneRigidActor(pxScene, *oldPxRigidActor);
         }
      }

      if (dynamic) {
         auto dynamic = GetPxRigidDynamic(pxRigidActor);
         dynamic->setLinearDamping(linearDamping);
         dynamic->setAngularDamping(angularDamping);
      }

      // todo: handle change
      if (destructible && tkActor == nullptr) {
         auto [trans, geom]  = entity.Get<SceneTransformComponent, GeometryComponent>();
         ASSERT(geom.type == GeomType::Box);

         ASSERT(!destructData);
         destructData = GetDestructData(trans.Scale());

         auto tkFramework = NvBlastTkFrameworkGet();

         TkActorDesc actorDesc;
         actorDesc.asset = destructData->tkAsset;
         actorDesc.uniformInitialBondHealth = 1;
         actorDesc.uniformInitialLowerSupportChunkHealth = 1;

         auto& physScene = GetPhysScene();
         tkActor = tkFramework->createActor(actorDesc);
         tkActor->getFamily().addListener(*physScene.destructEventListener);
         physScene.tkGroup->addActor(*tkActor);

         ASSERT(tkActor->userData == nullptr);
         tkActor->userData = new Entity{ entity }; // todo: use fixed allocator
      } else if (!destructible && tkActor != nullptr) {
         RemoveDestruct();
      }
   }

   void RigidBodyComponent::Remove() {
      RemoveDestruct();

      RemoveSceneRigidActor(*GetPhysScene().pxScene, *pxRigidActor);
      pxRigidActor = nullptr;
   }

   void RigidBodyComponent::RemoveDestruct() {
      if (!tkActor) {
         return;
      }

      auto& tkFamily = tkActor->getFamily();

      delete (Entity*)tkActor->userData;
      tkActor->userData = nullptr;
      PX_RELEASE(tkActor);

      if (tkFamily.getActorCount() == 0) {
         INFO("Destrcut actors count ZERO. Delete tkAsset");
         PX_RELEASE(destructData->tkAsset);
         delete destructData;
         destructData = nullptr;
      }
   }

   void RigidBodyComponent::AddShapesHier(const Entity& entity) {
      auto& trans = entity.Get<SceneTransformComponent>();

      if (entity.Has<RigidBodyShapeComponent>()) {
         const auto& geom = entity.Get<GeometryComponent>();

         PxGeometryHolder physGeom = GetPhysGeom(trans, geom);

         PxShape* shape = GetPxPhysics()->createShape(physGeom.any(), *GetPxMaterial(), true);

         // todo: do it by pbe::Transform
         PxTransform shapeTrans = GetTransform(trans);
         PxTransform actorTrans = pxRigidActor->getGlobalPose();
         PxTransform shapeOffset = actorTrans.transformInv(shapeTrans);

         shape->setLocalPose(shapeOffset);

         pxRigidActor->attachShape(*shape);
         shape->release();
      }

      for (auto& child : trans.children) {
         ASSERT(!child.Has<RigidBodyComponent>());
         AddShapesHier(child);
      }
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
      STRUCT_FIELD(destructible)
      STRUCT_FIELD(linearDamping)
      STRUCT_FIELD(angularDamping)
   STRUCT_END()

   STRUCT_BEGIN(RigidBodyShapeComponent)
   STRUCT_END()

   STRUCT_BEGIN(TriggerComponent)
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
   TYPER_REGISTER_COMPONENT(TriggerComponent);
   TYPER_REGISTER_COMPONENT(JointComponent);

}
