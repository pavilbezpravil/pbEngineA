#include "pch.h"
#include "DestructEventListener.h"

#include "PhysComponents.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "scene/Component.h"
#include "scene/Entity.h"

#include <NvBlastTk.h>
#include <NvBlastExtDamageShaders.h>

#include "core/CVar.h"
#include "scene/Utils.h"


using namespace Nv::Blast;

namespace pbe {

   static CVarValue<bool> cvAddTimedDieForLeaf{ "phys/add timed die for leaf", true };
   static CVarValue<bool> cvInstantDestroyLeafs{ "phys/instant destroy leafs", false };

   void DestructEventListener::receive(const TkEvent* events, uint32_t eventCount) {
      for (uint32_t i = 0; i < eventCount; ++i) {
         const TkEvent& event = events[i];

         switch (event.type) {
            case TkSplitEvent::EVENT_TYPE:
            {
               const TkSplitEvent* splitEvent = event.getPayload<TkSplitEvent>();

               auto parentEntity = *(Entity*)splitEvent->parentData.userData;
               delete (Entity*)splitEvent->parentData.userData;

               parentEntity.DestroyDelayed();

               auto parentTrans = parentEntity.GetTransform();
               auto& parentRb = parentEntity.Get<RigidBodyComponent>();
               parentRb.tkActor = nullptr; // parent was broken and now don't own tkActor
               auto destructData = parentRb.destructData;
               parentRb.destructData = nullptr;

               const auto& parentChunkToEntity = parentRb.chunkToEntity;
               ASSERT(!parentChunkToEntity.empty());

               const auto* chunks = destructData->tkAsset->getChunks();

               auto pScene = parentEntity.GetScene();

               for (uint32_t j = 0; j < splitEvent->numChildren; ++j) {
                  auto tkChild = splitEvent->children[j];

                  ASSERT_MT_UNIQUE();
                  uint32_t visibleChunkCount = tkChild->getVisibleChunkCount();
                  visibleChunkIndices.resize(visibleChunkCount);
                  tkChild->getVisibleChunkIndices(visibleChunkIndices.data(), visibleChunkCount);

                  Entity childEntity = pScene->Create(parentTrans.parent, "Chunk Dynamic");

                  auto& childTrans = childEntity.GetTransform();
                  childTrans.SetPosition(parentTrans.Position());
                  childTrans.SetRotation(parentTrans.Rotation());
                  vec3 childScale = childTrans.Scale();

                  bool childIsLeaf = visibleChunkCount == 1;
                  bool childIsLeafSupport = visibleChunkCount == 1;

                  std::unordered_map<uint, EntityID> childChunkToEntity(visibleChunkCount);

                  for (uint chunkIndex : visibleChunkIndices) {
                     const NvBlastChunk& chunk = chunks[chunkIndex];
                     auto& chunkInfo = destructData->chunkInfos[chunkIndex];

                     auto& offset = reinterpret_cast<const vec3&>(*chunk.centroid);

                     if (visibleChunkCount == 1) {
                        childIsLeaf = chunkInfo.isLeaf;
                        childIsLeafSupport = chunkInfo.isSupport;
                     }

                     Entity visibleChunkEntity = CreateCube(*pScene, CubeDesc {
                        .parent = childEntity,
                        .namePrefix = "Chunk Shape",
                        .space = Space::Local,
                        .pos = offset,
                        .scale = chunkInfo.size / childScale,
                        .type = CubeDesc::PhysShape,
                     });
                     // visibleChunkEntity.Add<DestructionChunkComponent>();

                     // todo: update prev transform for correct motion

                     childChunkToEntity[chunkIndex] = visibleChunkEntity.GetID();

                     uint visibleChunkParentIdx = chunkIndex;
                     while (!parentChunkToEntity.contains(visibleChunkParentIdx)) {
                        visibleChunkParentIdx = chunks[visibleChunkParentIdx].parentChunkIndex;
                        ASSERT(visibleChunkParentIdx != UINT32_MAX);
                     }

                     // todo: copy shape parametrs too
                     Entity visibleChunkParentEntity = { parentChunkToEntity.at(visibleChunkParentIdx), parentEntity.GetScene() };
                     visibleChunkEntity.Get<MaterialComponent>() = visibleChunkParentEntity.Get<MaterialComponent>();
                  }

                  // todo: mb do it not here, send event for example
                  if (cvAddTimedDieForLeaf && (childIsLeaf || childIsLeafSupport)) {
                     childEntity.Add<TimedDieComponent>().SetRandomDieTime(2.f, 5.f);
                  }

                  if (cvInstantDestroyLeafs && childIsLeaf) {
                     childEntity.DestroyDelayed(); // todo: dont create
                  }

                  // it may be destroyed TkActor with already delete userData
                  tkChild->userData = nullptr;

                  RigidBodyComponent childRb{};
                  childRb.dynamic = true;
                  // todo: mb make component for destructable object?
                  childRb.hardness = parentRb.hardness;

                  auto& childRb2 = childEntity.Add<RigidBodyComponent>(std::move(childRb));
                  childRb2.SetDestructible(*tkChild, *destructData);
                  childRb2.chunkToEntity = std::move(childChunkToEntity); // todo:

                  auto childDynamic = childRb2.pxRigidActor->is<PxRigidDynamic>();

                  auto parentDynamic = parentRb.pxRigidActor->is<PxRigidDynamic>();

                  if (parentDynamic) {
                     // todo: dont work with static parent
                     auto m_parentCOM = parentDynamic->getGlobalPose().transform(parentDynamic->getCMassLocalPose().p);
                     auto m_parentLinearVelocity = parentDynamic->getLinearVelocity();
                     auto m_parentAngularVelocity = parentDynamic->getAngularVelocity();

                     const PxVec3 COM = childDynamic->getGlobalPose().transform(childDynamic->getCMassLocalPose().p);
                     const PxVec3 linearVelocity = m_parentLinearVelocity + m_parentAngularVelocity.cross(COM - m_parentCOM);
                     const PxVec3 angularVelocity = m_parentAngularVelocity;
                     childDynamic->setLinearVelocity(linearVelocity);
                     childDynamic->setAngularVelocity(angularVelocity);
                  }
               }
            }
            break;

            case TkJointUpdateEvent::EVENT_TYPE:
               INFO("TkJointUpdateEvent");
               UNIMPLEMENTED();
            // {
            //    const TkJointUpdateEvent* jointEvent = event.getPayload<TkJointUpdateEvent>();  // Joint update event payload
            //
            //    // Joint events have three subtypes, see which one we have
            //    switch (jointEvent->subtype)
            //    {
            //    case TkJointUpdateEvent::External:
            //       myCreateJointFunction(jointEvent->joint);   // An internal joint has been "exposed" (now joins two different actors).  Create a physics joint.
            //       break;
            //    case TkJointUpdateEvent::Changed:
            //       myUpdatejointFunction(jointEvent->joint);   // A joint's actors have changed, so we need to update its corresponding physics joint.
            //       break;
            //    case TkJointUpdateEvent::Unreferenced:
            //       myDestroyJointFunction(jointEvent->joint);  // This joint is no longer referenced, so we may delete the corresponding physics joint.
            //       break;
            //    }
            // }
               break;

            // Unhandled:
            case TkFractureCommands::EVENT_TYPE:
               // INFO("TkFractureCommands");
               break;
            case TkFractureEvents::EVENT_TYPE:
               // INFO("TkFractureEvents");
               break;
            default:
               break;
         }
      }
   }

}
