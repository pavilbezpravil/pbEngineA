#include "pch.h"
#include "PhysicsScene.h"

#include "Phys.h"
#include "PhysComponents.h"
#include "PhysUtils.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "scene/Component.h"
#include "scene/Entity.h"
#include "PhysQuery.h"

#include <NvBlastTk.h>
#include <NvBlastExtDamageShaders.h>

#include "scene/Utils.h"


using namespace Nv::Blast;

namespace pbe {

   class DestructEventListener : public TkEventListener {
   public:
      void receive(const TkEvent* events, uint32_t eventCount) override {
         for (uint32_t i = 0; i < eventCount; ++i) {
            const TkEvent& event = events[i];

            switch (event.type) {
               case TkSplitEvent::EVENT_TYPE:
               {
                  const TkSplitEvent* splitEvent = event.getPayload<TkSplitEvent>();

                  ASSERT(splitEvent->parentData.userData);
                  auto entity = (Entity*)splitEvent->parentData.userData;
                  entity->DestroyDelayed();

                  auto parentTrans = entity->GetTransform();
                  auto& destruct = entity->Get<DestructComponent>();
                  destruct.releaseTkActor = false; // todo:

                  auto pScene = entity->GetScene();

                  for (uint32_t j = 0; j < splitEvent->numChildren; ++j) {
                     auto child = splitEvent->children[j];
                     // todo:
                     if (child->userData) {
                        child->userData = nullptr;
                     }
                     ASSERT(child->userData == nullptr);

                     uint32_t visibleChunkCount = child->getVisibleChunkCount();
                     std::array<uint, 32> visibleChunkIndices;
                     ASSERT(visibleChunkCount < visibleChunkIndices.size());
                     child->getVisibleChunkIndices(visibleChunkIndices.data(), visibleChunkCount);

                     Entity childEntity = pScene->Create(parentTrans.parent, "Chunk Dynamic");

                     for (uint iChunk = 0; iChunk < visibleChunkCount; ++iChunk) {
                        auto chunkIndex =  visibleChunkIndices[iChunk];
                        NvBlastChunk chunk = child->getAsset()->getChunks()[chunkIndex];

                        vec3 offset = vec3{ chunk.centroid[0], chunk.centroid[1], chunk.centroid[2] };

                        Entity visibleChunkEntity = CreateCube(*pScene, CubeDesc {
                           .parent = childEntity,
                           .namePrefix = "Chunk Shape",
                           .pos = offset,
                           .scale = destruct.destructData->chunkSizes[chunkIndex],
                           .type = CubeDesc::PhysShape,
                        });
                     }

                     childEntity.GetTransform().SetPosition(parentTrans.Position());
                     childEntity.GetTransform().SetRotation(parentTrans.Rotation());

                     // todo:
                     RigidBodyComponent _rb{};
                     _rb.dynamic = true;
                     childEntity.Add<RigidBodyComponent>(_rb);

                     {
                        DestructComponent childDestruct{};
                        childDestruct.root = false;
                        childDestruct.destructData = destruct.destructData;
                        childDestruct.tkActor = child;
                        childEntity.Add<DestructComponent>(childDestruct);
                     }
                  }

                  delete (Entity*)splitEvent->parentData.userData;
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

      PhysicsScene& scene;

      DestructEventListener(PhysicsScene& scene) : scene(scene) {}
   };

   PhysicsScene::PhysicsScene(Scene& scene) : scene(scene) {
      PxSceneDesc sceneDesc(GetPxPhysics()->getTolerancesScale());
      sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
      sceneDesc.cpuDispatcher = GetPxCpuDispatcher();
      sceneDesc.filterShader = PxDefaultSimulationFilterShader;
      // sceneDesc.filterShader = contactReportFilterShader;
      sceneDesc.simulationEventCallback = new SimulationEventCallback(this);
      sceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
      pxScene = GetPxPhysics()->createScene(sceneDesc);

      PxPvdSceneClient* pvdClient = pxScene->getScenePvdClient();
      if (pvdClient) {
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
      }

      TkGroupDesc groupDesc;
      groupDesc.workerCount = 1;
      tkGroup = NvBlastTkFrameworkGet()->createGroup(groupDesc);

      destructEventListener = std::make_unique<DestructEventListener>(*this);
   }

   PhysicsScene::~PhysicsScene() {
      ASSERT(tkGroup->getActorCount() == 0);
      PX_RELEASE(tkGroup);

      ASSERT(pxScene->getNbActors(PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC) == 0);
      delete pxScene->getSimulationEventCallback();
      PX_RELEASE(pxScene);
   }

   RayCastResult PhysicsScene::RayCast(const vec3& origin, const vec3& dir, float maxDistance) {
      PxVec3 pxOrigin = Vec3ToPx(origin);
      PxVec3 pxDir = Vec3ToPx(dir);
      PxRaycastBuffer hit;

      if (pxScene->raycast(pxOrigin, pxDir, maxDistance, hit, PxHitFlag::ePOSITION | PxHitFlag::eNORMAL)) {
         ASSERT(hit.hasBlock);
         ASSERT(hit.nbTouches == 0);
         if (hit.hasBlock) {
            return RayCastResult {
                  .physActor = *GetEntity(hit.block.actor),
                  .position = PxVec3ToPBE(hit.block.position),
                  .normal = PxVec3ToPBE(hit.block.normal),
                  .distance = hit.block.distance,
               };
         }
      }

      return RayCastResult{};
   }

   RayCastResult PhysicsScene::Sweep(const vec3& origin, const vec3& dir, float maxDistance) {
      PxVec3 pxOrigin = Vec3ToPx(origin);
      PxVec3 pxDir = Vec3ToPx(dir);

      PxSweepBuffer hit;
      // todo:
      auto geom = PxSphereGeometry(0.5f);
      if (pxScene->sweep(geom, PxTransform{ pxOrigin }, pxDir, maxDistance, hit, PxHitFlag::ePOSITION | PxHitFlag::eNORMAL)) {
         ASSERT(hit.hasBlock);
         ASSERT(hit.nbTouches == 0);
         if (hit.hasBlock) {
            return RayCastResult{
                  .physActor = *GetEntity(hit.block.actor),
                  .position = PxVec3ToPBE(hit.block.position),
                  .normal = PxVec3ToPBE(hit.block.normal),
                  .distance = hit.block.distance,
            };
         }
      }

      return RayCastResult{};
   }

   void PhysicsScene::SyncPhysicsWithScene() {
      for (auto [_, trans, trigger] :
         scene.View<SceneTransformComponent, TriggerComponent, TransformChangedMarker>().each()) {
         trigger.pxRigidActor->setGlobalPose(GetTransform(trans));
      }

      for (auto [_, trans, rb] :
         scene.View<SceneTransformComponent, RigidBodyComponent, TransformChangedMarker>().each()) {
         rb.pxRigidActor->setGlobalPose(GetTransform(trans));
         PxWakeUp(rb.pxRigidActor);
      }
   }

   void PhysicsScene::Simulate(float dt) {
      PROFILE_CPU("Phys simulate");

      int steps = stepTimer.Update(dt);
      if (steps > 2) {
         stepTimer.Reset();
         steps = 2;
      }

      for (int i = 0; i < steps; ++i) {
         pxScene->simulate(stepTimer.GetActTime());
         pxScene->fetchResults(true);
         UpdateSceneAfterPhysics();

         // todo: or before simulate?
         tkGroup->process();

         scene.DestroyDelayedEntities();
      }
   }

   void PhysicsScene::UpdateSceneAfterPhysics() {
      PxU32 nbActiveActors;
      PxActor** activeActors = pxScene->getActiveActors(nbActiveActors);

      for (PxU32 i = 0; i < nbActiveActors; ++i) {
         Entity entity = *static_cast<Entity*>(activeActors[i]->userData);
         auto& trans = entity.Get<SceneTransformComponent>();

         PxRigidActor* rbActor = activeActors[i]->is<PxRigidActor>();
         ASSERT_MESSAGE(rbActor, "It must be rigid actor");
         if (rbActor) {
            PxTransform pxTrans = rbActor->getGlobalPose();
            trans.SetPosition(PxVec3ToPBE(pxTrans.p));
            trans.SetRotation(PxQuatToPBE(pxTrans.q));
         }
      }
   }

   void PhysicsScene::OnSetEventHandlers(entt::registry& registry) {
      registry.on_construct<RigidBodyComponent>().connect<&PhysicsScene::OnConstructRigidBody>(this);
      registry.on_destroy<RigidBodyComponent>().connect<&PhysicsScene::OnDestroyRigidBody>(this);
      registry.on_update<RigidBodyComponent>().connect<&PhysicsScene::OnUpdateRigidBody>(this);

      registry.on_construct<DestructComponent>().connect<&PhysicsScene::OnConstructDestruct>(this);
      registry.on_destroy<DestructComponent>().connect<&PhysicsScene::OnDestroyDestruct>(this);
      // registry.on_update<DestructComponent>().connect<&PhysicsScene::OnUpdateDestruct>(this);

      registry.on_construct<TriggerComponent>().connect<&PhysicsScene::OnConstructTrigger>(this);
      registry.on_destroy<TriggerComponent>().connect<&PhysicsScene::OnDestroyTrigger>(this);

      registry.on_construct<JointComponent>().connect<&PhysicsScene::OnConstructJoint>(this);
      registry.on_destroy<JointComponent>().connect<&PhysicsScene::OnDestroyJoint>(this);
      registry.on_update<JointComponent>().connect<&PhysicsScene::OnUpdateJoint>(this);
   }

   void PhysicsScene::OnEntityEnable() {
      for (auto e : pScene->ViewAll<GeometryComponent, RigidBodyComponent, DelayedEnableMarker>()) {
         Entity entity{ e, &scene };
         AddRigidActor(entity);
      }

      for (auto e : pScene->ViewAll<DestructComponent, DelayedEnableMarker>()) {
         Entity entity{ e, &scene };
         AddDestructActor(entity);
      }

      for (auto e : pScene->ViewAll<GeometryComponent, TriggerComponent, DelayedEnableMarker>()) {
         Entity entity{ e, &scene };
         AddTrigger(entity);
      }

      for (auto e : pScene->ViewAll<JointComponent, DelayedEnableMarker>()) {
         Entity entity{ e, &scene };
         AddJoint(entity);
      }
   }

   void PhysicsScene::OnEntityDisable() {
      for (auto e : pScene->ViewAll<RigidBodyComponent, DelayedDisableMarker>()) {
         Entity entity{ e, &scene };
         RemoveRigidActor(entity);
      }

      for (auto e : pScene->ViewAll<DestructComponent, DelayedDisableMarker>()) {
         Entity entity{ e, &scene };
         RemoveDestructActor(entity);
      }

      for (auto e : pScene->ViewAll<TriggerComponent, DelayedDisableMarker>()) {
         Entity entity{ e, &scene };
         RemoveTrigger(entity);
      }

      for (auto e : pScene->ViewAll<JointComponent, DelayedDisableMarker>()) {
         Entity entity{ e, &scene };
         RemoveJoint(entity);
      }
   }

   void PhysicsScene::OnUpdate(float dt) {
      Simulate(dt);
   }

   static void AddShapes(Entity& entity, PxRigidActor* actor) {
      auto& trans = entity.Get<SceneTransformComponent>();

      if (entity.Has<RigidBodyShapeComponent>()) {
         const auto& geom = entity.Get<GeometryComponent>();

         PxGeometryHolder physGeom = GetPhysGeom(trans, geom);

         PxShape* shape = GetPxPhysics()->createShape(physGeom.any(), *GetPxMaterial(), true);

         // todo: do it by pbe::Transform
         PxTransform shapeTrans = GetTransform(trans);
         PxTransform actorTrans = actor->getGlobalPose();
         PxTransform shapeOffset = actorTrans.transformInv(shapeTrans);

         shape->setLocalPose(shapeOffset);

         actor->attachShape(*shape);
         shape->release();
      }

      for (auto& child : trans.children) {
         ASSERT(!child.Has<RigidBodyComponent>());
         AddShapes(child, actor);
      }
   }

   static PxRigidActor* CreateSceneRigidActor(PxScene* pxScene, Entity entity) {
      // todo: pass as function argument
      auto [trans, rb] = entity.Get<SceneTransformComponent, RigidBodyComponent>();

      PxTransform physTrans = GetTransform(trans);

      PxRigidActor* actor = nullptr;
      if (rb.dynamic) {
         actor = GetPxPhysics()->createRigidDynamic(physTrans);
      } else {
         actor = GetPxPhysics()->createRigidStatic(physTrans);
      }

      // todo: tmp, remove after reconvert all scenes
      if (entity.Has<GeometryComponent>()) {
         entity.AddOrReplace<RigidBodyShapeComponent>();
      }

      AddShapes(entity, actor);

      if (rb.dynamic) {
         float density = 10.f; // todo:
         PxRigidBodyExt::updateMassAndInertia(*GetPxRigidDynamic(actor), density);
      }

      actor->userData = new Entity{ entity }; // todo: use fixed allocator
      pxScene->addActor(*actor);

      return actor;
   }

   void RemoveSceneRigidActor(PxScene* pxScene, PxRigidActor* pxRigidActor) {
      pxScene->removeActor(*pxRigidActor);
      delete (Entity*)pxRigidActor->userData;
      pxRigidActor->userData = nullptr;
   }

   void PhysicsScene::AddRigidActor(Entity entity) {
      // todo: pass as function argument
      auto& rb = entity.Get<RigidBodyComponent>();
      PxRigidActor* actor = CreateSceneRigidActor(pxScene, entity);

      ASSERT(!rb.pxRigidActor);
      rb.pxRigidActor = actor;

      rb.SetData();
   }

   void PhysicsScene::RemoveRigidActor(Entity entity) {
      auto& rb = entity.Get<RigidBodyComponent>();
      if (!rb.pxRigidActor) {
         return;
      }
      RemoveSceneRigidActor(pxScene, rb.pxRigidActor);
      rb.pxRigidActor = nullptr;
   }

   void PhysicsScene::UpdateRigidActor(Entity entity) {
      auto& rb = entity.Get<RigidBodyComponent>();
      ASSERT(rb.pxRigidActor);

      bool isDynamic = rb.pxRigidActor->is<PxRigidDynamic>();
      bool isDynamicChanged = isDynamic != rb.dynamic;
      if (isDynamicChanged) {
         PxRigidActor* newActor = CreateSceneRigidActor(pxScene, entity);

         PxU32 nbConstrains = rb.pxRigidActor->getNbConstraints();

         if (nbConstrains) {
            PxConstraint* constrains[8];
            ASSERT(nbConstrains <= _countof(constrains));
            nbConstrains = std::min(nbConstrains, (uint)_countof(constrains));
            rb.pxRigidActor->getConstraints(constrains, nbConstrains);

            for (PxU32 i = 0; i < nbConstrains; i++) {
               auto pxConstrain = constrains[i];
               auto pEntity = (Entity*)pxConstrain->userData;
               // todo: mb PxConstraint::userData should be PxJoint?
               auto pxJoint = pEntity->Get<JointComponent>().pxJoint;

               PxRigidActor* actor0, * actor1;
               pxJoint->getActors(actor0, actor1);

               if (actor0 == rb.pxRigidActor) {
                  pxJoint->setActors(newActor, actor1);
               } else {
                  ASSERT(actor1 == rb.pxRigidActor);
                  pxJoint->setActors(actor0, newActor);
               }
            }
         }

         RemoveSceneRigidActor(pxScene, rb.pxRigidActor);
         rb.pxRigidActor = newActor;
      }

      rb.SetData();
   }

   static DestructData* GetDestructData(const SceneTransformComponent& trans, const DestructComponent& destruct) {
      vec3 chunkSize = trans.Scale();

      uint slices = std::max(1, (int)chunkSize.x);
      uint chunkCount = 1 + slices + 1;
      uint bondCount = slices;

      std::vector<NvBlastChunkDesc> chunkDescs;
      chunkDescs.resize(chunkCount);

      std::vector<vec3> chunkSizes;
      chunkSizes.resize(chunkCount);

      // parent
      chunkDescs[0].parentChunkDescIndex = UINT32_MAX; // invalid index denotes a chunk hierarchy root
      chunkDescs[0].centroid[0] = 0.0f;
      chunkDescs[0].centroid[1] = 0.0f;
      chunkDescs[0].centroid[2] = 0.0f;
      chunkDescs[0].volume = 1.0f;
      chunkDescs[0].flags = NvBlastChunkDesc::NoFlags;
      chunkDescs[0].userData = 0;

      chunkSizes[0] = chunkSize;

      std::vector<NvBlastBondDesc> bondDescs;
      bondDescs.resize(bondCount);

      uint boundIdx = 0;

      float chunkParentVolume = chunkSize.x * chunkSize.y * chunkSize.z;
      float chunkParentSliceAxisSize = chunkSize.x;
      float sliceAxisStart = -chunkSize.x * 0.5f; // todo:
      float chunkSliceAxisSize = chunkParentSliceAxisSize / (slices + 1);
      float chunkVolume = chunkParentVolume / (slices + 1);
      uint parentChunkIdx = 0;
      uint chunkIdx = 1;
      for (int i = 0; i <= slices; ++i) {
         auto& chunkDesc = chunkDescs[chunkIdx];
         chunkDesc.parentChunkDescIndex = parentChunkIdx;
         chunkDesc.centroid[0] = sliceAxisStart + chunkSliceAxisSize * (0.5f + i);
         chunkDesc.centroid[1] = 0.0f;
         chunkDesc.centroid[2] = 0.0f;
         chunkDesc.volume = chunkVolume;
         chunkDesc.flags = NvBlastChunkDesc::SupportFlag;
         chunkDesc.userData = chunkIdx;

         chunkSizes[chunkIdx] = vec3{ chunkSliceAxisSize, chunkSize.y, chunkSize.z };

         ++chunkIdx;

         if (i < slices) {
            auto& boundDesc = bondDescs[boundIdx++];

            boundDesc.chunkIndices[0] = chunkIdx - 1; // chunkIndices refer to chunk descriptor indices for support chunks
            boundDesc.chunkIndices[1] = chunkIdx;
            boundDesc.bond.normal[0] = 1.0f;
            boundDesc.bond.normal[1] = 0.0f;
            boundDesc.bond.normal[2] = 0.0f;
            boundDesc.bond.area = chunkSize.y * chunkSize.z; // todo:
            boundDesc.bond.centroid[0] = sliceAxisStart + chunkSliceAxisSize * (i + 1); // todo:
            boundDesc.bond.centroid[1] = 0.0f;
            boundDesc.bond.centroid[2] = 0.0f;
         }
      }

      // bondDescs[0].userData = 0;  // this can be used to tell the user more information about this
      // bond for example to create a joint when this bond breaks

      // bondDescs[1].chunkIndices[0] = 1;
      // bondDescs[1].chunkIndices[1] = ~0;  // ~0 (UINT32_MAX) is the "invalid index."  This creates a world bond
      // ... etc. for bondDescs[1], all other fields are filled in as usual

      TkAssetDesc assetDesc;
      assetDesc.chunkCount = chunkCount;
      assetDesc.chunkDescs = chunkDescs.data();
      assetDesc.bondCount = bondCount;
      assetDesc.bondDescs = bondDescs.data();
      assetDesc.bondFlags = nullptr;

      auto tkFramework = NvBlastTkFrameworkGet();

      bool wasCoverage = tkFramework->ensureAssetExactSupportCoverage(chunkDescs.data(), assetDesc.chunkCount);
      INFO("Was coverage {}", wasCoverage);

      // todo: 'chunkReorderMap' may be skipped
      std::vector<uint32_t> chunkReorderMap(chunkCount);  // Will be filled with a map from the original chunk descriptor order to the new one
      bool requireReordering = !tkFramework->reorderAssetDescChunks(chunkDescs.data(), assetDesc.chunkCount, bondDescs.data(), assetDesc.bondCount, chunkReorderMap.data());
      INFO("Require reordering {}", requireReordering);

      TkAsset* tkAsset = tkFramework->createAsset(assetDesc);

      // todo:
      DestructData* destructData = new DestructData{};
      destructData->tkAsset = tkAsset;
      destructData->chunkSizes = std::move(chunkSizes);
      return destructData;
   }

   void PhysicsScene::AddDestructActor(Entity entity) {
      // todo: pass as function argument
      auto [trans, destruct] = entity.Get<SceneTransformComponent, DestructComponent>();

      if (destruct.root) {
         auto& geom = entity.Get<GeometryComponent>();
         ASSERT(geom.type == GeomType::Box);

         auto destructData = GetDestructData(trans, destruct);
         destruct.destructData = destructData;

         auto tkFramework = NvBlastTkFrameworkGet();

         TkActorDesc actorDesc;
         actorDesc.asset = destruct.destructData->tkAsset;
         actorDesc.uniformInitialBondHealth = 1;
         actorDesc.uniformInitialLowerSupportChunkHealth = 1;

         TkActor* tkActor = tkFramework->createActor(actorDesc);
         tkActor->getFamily().addListener(*destructEventListener);
         tkGroup->addActor(*tkActor);

         destruct.tkActor = tkActor;
      } else {
         ASSERT(destruct.destructData);
         ASSERT(destruct.tkActor);
      }

      ASSERT(destruct.tkActor->userData == nullptr);
      destruct.tkActor->userData = new Entity{ entity }; // todo: use fixed allocator
   }

   void PhysicsScene::RemoveDestructActor(Entity entity) {
      auto& destruct = entity.Get<DestructComponent>();

      auto& tkFamily = destruct.tkActor->getFamily();
      // auto actorsCount = destruct.tkActor->getFamily().getActorCount();
      // INFO("Destuct actors before release TkActor {}", destruct.tkActor->getFamily().getActorCount());

      if (destruct.releaseTkActor) {
         delete (Entity*)destruct.tkActor->userData;
         destruct.tkActor->userData = nullptr;
         PX_RELEASE(destruct.tkActor);
      }

      // todo: do not release tkAsset if other actors are exist
      if (tkFamily.getActorCount() == 0) {
         INFO("Destrcut actors count ZERO. Delete tkAsset");
         PX_RELEASE(destruct.destructData->tkAsset);
         delete destruct.destructData;
         destruct.destructData = nullptr;
      }
   }

   void PhysicsScene::AddTrigger(Entity entity) {
      auto [trans, geom, trigger] = entity.Get<SceneTransformComponent, GeometryComponent, TriggerComponent>();

      PxGeometryHolder physGeom = GetPhysGeom(trans, geom);

      const PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eTRIGGER_SHAPE;
      PxShape* shape = GetPxPhysics()->createShape(physGeom.any(), *GetPxMaterial(), true, shapeFlags);

      PxRigidStatic* actor = GetPxPhysics()->createRigidStatic(GetTransform(trans));
      actor->attachShape(*shape);
      actor->userData = new Entity{ entity }; // todo: use fixed allocator
      pxScene->addActor(*actor);
      shape->release();

      trigger.pxRigidActor = actor;
   }

   void PhysicsScene::RemoveTrigger(Entity entity) {
      auto& trigger = entity.Get<TriggerComponent>();
      if (!trigger.pxRigidActor) {
         return;
      }

      pxScene->removeActor(*trigger.pxRigidActor);
      delete (Entity*)trigger.pxRigidActor->userData;
      trigger.pxRigidActor->userData = nullptr;

      trigger.pxRigidActor = nullptr;
   }

   void PhysicsScene::AddJoint(Entity entity) {
      auto& joint = entity.Get<JointComponent>();
      joint.pxJoint = nullptr; // todo: ctor copy this by value
      joint.SetData(entity);
   }

   void PhysicsScene::RemoveJoint(Entity entity) {
      auto& joint = entity.Get<JointComponent>();
      if (!joint.pxJoint) {
         return;
      }

      joint.WakeUp();

      delete (Entity*)joint.pxJoint->userData;
      joint.pxJoint->getConstraint()->userData = nullptr;
      joint.pxJoint->userData = nullptr;

      joint.pxJoint->release();
      joint.pxJoint = nullptr;
      
   }

   void PhysicsScene::OnConstructRigidBody(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      // todo: when copy component copy ptr to
      entity.Get<RigidBodyComponent>().pxRigidActor = nullptr;

      // todo: on each func has this check
      if (entity.Enabled()) {
         AddRigidActor(entity);
      }
   }

   void PhysicsScene::OnDestroyRigidBody(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      RemoveRigidActor(entity);
   }

   void PhysicsScene::OnUpdateRigidBody(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      if (entity.Enabled()) {
         UpdateRigidActor(entity);
      }
   }

   void PhysicsScene::OnConstructDestruct(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      // todo: when copy component copy ptr to
      // entity.Get<DestructComponent>().pxRigidActor = nullptr;

      // todo: on each func has this check
      if (entity.Enabled()) {
         AddDestructActor(entity);
      }
   }

   void PhysicsScene::OnDestroyDestruct(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      RemoveDestructActor(entity);
   }

   void PhysicsScene::OnUpdateDestruct(entt::registry& registry, entt::entity _entity) {
      // Entity entity{ _entity, &scene };
      // if (entity.Enabled()) {
      //    UpdateRigidActor(entity);
      // }
   }

   void PhysicsScene::OnConstructTrigger(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      entity.Get<TriggerComponent>().pxRigidActor = nullptr; // todo:
      if (entity.Enabled()) {
         AddTrigger(entity);
      }
   }

   void PhysicsScene::OnDestroyTrigger(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      RemoveTrigger(entity);
   }

   void PhysicsScene::OnConstructJoint(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      entity.Get<JointComponent>().pxJoint = nullptr; // todo:
      if (entity.Enabled()) {
         AddJoint(entity);
      }
   }

   void PhysicsScene::OnDestroyJoint(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      RemoveJoint(entity);
   }

   void PhysicsScene::OnUpdateJoint(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };
      if (entity.Enabled()) {
         entity.Get<JointComponent>().SetData(entity);
      }
   }

}
