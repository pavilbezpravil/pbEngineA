#include "pch.h"
#include "PhysicsScene.h"

#include "PhysComponents.h"
#include "PhysUtils.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "scene/Component.h"
#include "scene/Entity.h"
#include "PhysQuery.h"


namespace pbe {

#define PVD_HOST "127.0.0.1"	//Set this to the IP address of the system running the PhysX Visual Debugger that you want to connect to.

   static PxDefaultAllocator gAllocator;
   static PxDefaultErrorCallback	gErrorCallback;
   static PxFoundation* gFoundation = NULL;
   static PxPhysics* gPhysics = NULL;
   static PxDefaultCpuDispatcher* gDispatcher = NULL;
   static PxMaterial* gMaterial = NULL;
   static PxPvd* gPvd = NULL;

   void InitPhysics() {
      gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);

      gPvd = PxCreatePvd(*gFoundation);
      PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
      gPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

      gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true, gPvd);
      gDispatcher = PxDefaultCpuDispatcherCreate(2);
      gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.25f);
   }

   void TermPhysics() {
      PX_RELEASE(gDispatcher);
      PX_RELEASE(gPhysics);
      if (gPvd) {
         PxPvdTransport* transport = gPvd->getTransport();
         gPvd->release();	gPvd = NULL;
         PX_RELEASE(transport);
      }
      PX_RELEASE(gFoundation);
   }

   // todo: delete?
   PxFilterFlags contactReportFilterShader(
         PxFilterObjectAttributes attributes0, PxFilterData filterData0,
         PxFilterObjectAttributes attributes1, PxFilterData filterData1,
         PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize) {
      PX_UNUSED(attributes0);
      PX_UNUSED(attributes1);
      PX_UNUSED(filterData0);
      PX_UNUSED(filterData1);
      PX_UNUSED(constantBlockSize);
      PX_UNUSED(constantBlock);

      if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1)) {
         pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
         return PxFilterFlag::eDEFAULT;
      }

      // todo:
      pairFlags = PxPairFlag::eCONTACT_DEFAULT
                | PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_LOST
                | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_CONTACT_POINTS;
      return PxFilterFlag::eDEFAULT;
   }

   struct SimulationEventCallback : PxSimulationEventCallback {
      PhysicsScene* physScene{};
      SimulationEventCallback(PhysicsScene* physScene) : physScene(physScene) {}

      void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override {}

      void onWake(PxActor** actors, PxU32 count) override {
         for (PxU32 i = 0; i < count; i++) {
            Entity& e = *GetEntity(actors[i]);
            INFO("Wake event {}", e.GetName());
         }
      }

      void onSleep(PxActor** actors, PxU32 count) override {
         for (PxU32 i = 0; i < count; i++) {
            Entity& e = *GetEntity(actors[i]);
            INFO("Sleep event {}", e.GetName());
         }
      }

      void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override {
         for (PxU32 i = 0; i < nbPairs; i++) {
            const PxContactPair& cp = pairs[i];

            Entity& e0 = *GetEntity(pairHeader.actors[0]);
            Entity& e1 = *GetEntity(pairHeader.actors[1]);

            if (cp.events & PxPairFlag::eNOTIFY_TOUCH_FOUND) {
               INFO("onContact eNOTIFY_TOUCH_FOUND {} {}", e0.GetName(), e1.GetName());
               // physScene->scene.DispatchEvent<ContactEnterEvent>(e0, e1);
            } else if (cp.events & PxPairFlag::eNOTIFY_TOUCH_LOST) {
               INFO("onContact eNOTIFY_TOUCH_LOST {} {}", e0.GetName(), e1.GetName());
               // physScene->scene.DispatchEvent<ContactExitEvent>(e0, e1);
            }
         }
      }

      void onTrigger(PxTriggerPair* pairs, PxU32 count) override {
         for (PxU32 i = 0; i < count; i++) {
            const PxTriggerPair& pair = pairs[i];
            if (pair.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER)) {
               continue;
            }

            Entity& e0 = *GetEntity(pair.triggerActor);
            Entity& e1 = *GetEntity(pair.otherActor);

            if (pair.status & PxPairFlag::eNOTIFY_TOUCH_FOUND) {
               INFO("onTrigger eNOTIFY_TOUCH_FOUND {} {}", e0.GetName(), e1.GetName());
               e1.DestroyDelayed();
               // physScene->scene.DispatchEvent<TriggerEnterEvent>(e0, e1);
            } else if (pair.status & PxPairFlag::eNOTIFY_TOUCH_LOST) {
               INFO("onTrigger eNOTIFY_TOUCH_LOST {} {}", e0.GetName(), e1.GetName());
               // physScene->scene.DispatchEvent<TriggerExitEvent>(e0, e1);
            }
         }
      }

      // todo: advance?
      void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override {}
   };

   PhysicsScene::PhysicsScene(Scene& scene) : scene(scene) {
      PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
      sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
      sceneDesc.cpuDispatcher = gDispatcher;
      sceneDesc.filterShader = PxDefaultSimulationFilterShader;
      // sceneDesc.filterShader = contactReportFilterShader;
      sceneDesc.simulationEventCallback = new SimulationEventCallback(this);
      sceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
      pxScene = gPhysics->createScene(sceneDesc);

      PxPvdSceneClient* pvdClient = pxScene->getScenePvdClient();
      if (pvdClient) {
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
         pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
      }
   }

   PhysicsScene::~PhysicsScene() {
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
      // todo: only for changed entities

      for (auto [_, trans, rb] :
         scene.View<SceneTransformComponent, TriggerComponent>().each()) {
         rb.pxRigidActor->setGlobalPose(GetTransform(trans));
      }

      // for (auto [_, trans, rb] :
      //    scene.View<SceneTransformComponent, RigidBodyComponent>().each()) {
      //    if (rb.dynamic) {
      //       continue;
      //    }
      //
      //    rb.pxRigidActor->setGlobalPose(GetTransform(trans));
      // }
   }

   void PhysicsScene::Simulate(float dt) {
      PROFILE_CPU("Phys simulate");

      SyncPhysicsWithScene();

      int steps = stepTimer.Update(dt);
      if (steps > 2) {
         stepTimer.Reset();
         steps = 2;
      }

      for (int i = 0; i < steps; ++i) {
         pxScene->simulate(stepTimer.GetActTime());
         pxScene->fetchResults(true);

         UpdateSceneAfterPhysics();

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

   static PxRigidActor* CreateSceneRigidActor(PxScene* pxScene, Entity entity) {
      // todo: pass as function argument
      auto [trans, geom, rb] = entity.Get<SceneTransformComponent, GeometryComponent, RigidBodyComponent>();

      PxGeometryHolder physGeom = GetPhysGeom(trans, geom);
      PxTransform physTrans = GetTransform(trans);

      PxRigidActor* actor = nullptr;
      if (rb.dynamic) {
         // todo: density, damping
         actor = PxCreateDynamic(*gPhysics, physTrans, physGeom.any(), *gMaterial, 10.0f);
      } else {
         actor = PxCreateStatic(*gPhysics, physTrans, physGeom.any(), *gMaterial);
      }

      actor->userData = new Entity{ entity }; // todo: use fixed allocator
      pxScene->addActor(*actor);

      return actor;
   }

   void RemoveSceneRigidActor(PxScene* pxScene, RigidBodyComponent& rb) {
      pxScene->removeActor(*rb.pxRigidActor);
      delete GetEntity(rb.pxRigidActor);
      rb.pxRigidActor->userData = nullptr;

      rb.pxRigidActor = nullptr;
   }

   void PhysicsScene::AddRigidActor(Entity entity) {
      // todo: pass as function argument
      auto [trans, geom, rb] = entity.Get<SceneTransformComponent, GeometryComponent, RigidBodyComponent>();
      PxRigidActor* actor = CreateSceneRigidActor(pxScene, entity);

      ASSERT(!rb.pxRigidActor);
      rb.pxRigidActor = actor;

      rb.SetData();
   }

   void PhysicsScene::RemoveRigidActor(Entity entity) {
      auto& rb = entity.Get<RigidBodyComponent>();
      ASSERT(rb.pxRigidActor);
      RemoveSceneRigidActor(pxScene, rb);
   }

   void PhysicsScene::UpdateRigidActor(Entity entity) {
      auto& rb = entity.Get<RigidBodyComponent>();
      ASSERT(rb.pxRigidActor);

      bool isDynamic = rb.pxRigidActor->is<PxRigidDynamic>();
      bool isDynamicChanged = isDynamic != rb.dynamic;
      if (isDynamicChanged) {
         PxRigidActor* newActor = CreateSceneRigidActor(pxScene, entity);

         {
            PxU32 jointCount = rb.pxRigidActor->getNbConstraints();

            PxConstraint** joints = new PxConstraint* [jointCount]; // todo: allocation
            rb.pxRigidActor->getConstraints(joints, jointCount);

            for (PxU32 i = 0; i < jointCount; i++) {
               auto pxJoint = joints[i];

               PxRigidActor *actor0, *actor1;
               pxJoint->getActors(actor0, actor1);

               Entity e0 = *GetEntity(actor0);
               Entity e1 = *GetEntity(actor1);

               if (actor0 == rb.pxRigidActor) {
                  pxJoint->setActors(newActor, actor1);
                  PxWakeUp(actor1);
               } else {
                  ASSERT(actor1 == rb.pxRigidActor);
                  pxJoint->setActors(actor0, newActor);
                  PxWakeUp(actor0);
               }

               PxWakeUp(actor0);
               PxWakeUp(actor1);
               PxWakeUp(newActor);
            }

            delete[] joints;
         }

         RemoveSceneRigidActor(pxScene, rb);

         rb.pxRigidActor = newActor;
      }

      rb.SetData();
   }

   void PhysicsScene::AddTrigger(Entity entity) {
      auto [trans, geom, trigger] = entity.Get<SceneTransformComponent, GeometryComponent, TriggerComponent>();

      PxGeometryHolder physGeom = GetPhysGeom(trans, geom);

      const PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eTRIGGER_SHAPE;
      PxShape* shape = gPhysics->createShape(physGeom.any(), *gMaterial, true, shapeFlags);

      PxRigidStatic* actor = gPhysics->createRigidStatic(GetTransform(trans));
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
      delete GetEntity(trigger.pxRigidActor);
      trigger.pxRigidActor->userData = nullptr;

      trigger.pxRigidActor = nullptr;
   }

   void PhysicsScene::AddJoint(Entity entity) {
      auto& joint = entity.Get<JointComponent>();
      joint.pxJoint = nullptr; // todo: ctor copy this by value
      joint.SetData(gPhysics);
   }

   void PhysicsScene::RemoveJoint(Entity entity) {
      auto& joint = entity.Get<JointComponent>();
      if (!joint.pxJoint) {
         return;
      }

      joint.WakeUp();

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
         entity.Get<JointComponent>().SetData(gPhysics);
      }
   }

   PxPhysics* GetPxPhysics() {
      return gPhysics;
   }

}
