#include "pch.h"
#include "DestructEventListener.h"

#include "PhysComponents.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "scene/Component.h"
#include "scene/Entity.h"

#include <NvBlastTk.h>
#include <NvBlastExtDamageShaders.h>

#include "scene/Utils.h"


using namespace Nv::Blast;

namespace pbe {

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

               auto pScene = parentEntity.GetScene();

               for (uint32_t j = 0; j < splitEvent->numChildren; ++j) {
                  auto tkChild = splitEvent->children[j];

                  std::array<uint, 64> visibleChunkIndices; // todo:

                  uint32_t visibleChunkCount = tkChild->getVisibleChunkCount();
                  ASSERT(visibleChunkCount < visibleChunkIndices.size());
                  tkChild->getVisibleChunkIndices(visibleChunkIndices.data(), visibleChunkCount);

                  Entity childEntity = pScene->Create(parentTrans.parent, "Chunk Dynamic");

                  for (uint iChunk = 0; iChunk < visibleChunkCount; ++iChunk) {
                     auto chunkIndex =  visibleChunkIndices[iChunk];
                     NvBlastChunk chunk = tkChild->getAsset()->getChunks()[chunkIndex];

                     vec3 offset = vec3{ chunk.centroid[0], chunk.centroid[1], chunk.centroid[2] };

                     Entity visibleChunkEntity = CreateCube(*pScene, CubeDesc {
                        .parent = childEntity,
                        .namePrefix = "Chunk Shape",
                        .pos = offset,
                        .scale = destructData->chunkSizes[chunkIndex],
                        .type = CubeDesc::PhysShape,
                     });
                  }

                  childEntity.GetTransform().SetPosition(parentTrans.Position());
                  childEntity.GetTransform().SetRotation(parentTrans.Rotation());

                  // it may be destroyed TkActor with already delete userData
                  tkChild->userData = nullptr;

                  RigidBodyComponent childRb{};
                  childRb.dynamic = true;

                  auto& childRb2 = childEntity.Add<RigidBodyComponent>(std::move(childRb));
                  childRb2.SetDestructible(*tkChild, *destructData);

                  auto childDynamic = childRb2.pxRigidActor->is<PxRigidDynamic>();

                  auto parentDynamic = parentRb.pxRigidActor->is<PxRigidDynamic>();

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
