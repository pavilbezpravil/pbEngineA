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

               auto pScene = parentEntity.GetScene();

               for (uint32_t j = 0; j < splitEvent->numChildren; ++j) {
                  auto tkChild = splitEvent->children[j];

                  // it may be destroyed TkActor with already delete userData
                  tkChild->userData = nullptr;

                  std::array<uint, 32> visibleChunkIndices;

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
                        .scale = parentRb.destructData->chunkSizes[chunkIndex],
                        .type = CubeDesc::PhysShape,
                     });
                  }

                  childEntity.GetTransform().SetPosition(parentTrans.Position());
                  childEntity.GetTransform().SetRotation(parentTrans.Rotation());

                  RigidBodyComponent childRb{};
                  childRb.dynamic = true;
                  childRb.tkActor = tkChild;
                  childRb.destructData = parentRb.destructData;
                  parentRb.destructData = nullptr;

                  childEntity.Add<RigidBodyComponent>(std::move(childRb));
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
