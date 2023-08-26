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


namespace pbe {

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
      for (auto [_, trans, trigger] :
         scene.View<SceneTransformComponent, TriggerComponent, TransformChangedMarker>().each()) {
         trigger.pxRigidActor->setGlobalPose(GetTransform(trans));
      }

      for (auto [_, trans, rb] :
         scene.View<SceneTransformComponent, RigidBodyComponent, TransformChangedMarker>().each()) {
         rb.pxRigidActor->setGlobalPose(GetTransform(trans));
         PxWakeUp(rb.pxRigidActor);
      }

      // todo:
      scene.ClearComponent<TransformChangedMarker>();
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
         actor = PxCreateDynamic(*GetPxPhysics(), physTrans, physGeom.any(), *GetPxMaterial(), 10.0f);
      } else {
         actor = PxCreateStatic(*GetPxPhysics(), physTrans, physGeom.any(), *GetPxMaterial());
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
      auto [trans, geom, rb] = entity.Get<SceneTransformComponent, GeometryComponent, RigidBodyComponent>();
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
