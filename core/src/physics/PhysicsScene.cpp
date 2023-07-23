#include "pch.h"
#include "PhysicsScene.h"
#include "PhysXTypeConvet.h"
#include "core/Profiler.h"
#include "scene/Component.h"
#include "scene/Entity.h"


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
      PX_RELEASE(pxScene);
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

   void PhysicsScene::Simulation(float dt) {
      PROFILE_CPU("Phys simulate");

      SyncPhysicsWithScene();

      timeAccumulator += dt;

      // todo: config
      const float fixedDt = 1.0f / 60.0f;
      int steps = (int)(timeAccumulator / fixedDt);
      timeAccumulator -= steps * fixedDt;

      if (steps > 2) {
         timeAccumulator = 0;
         steps = 2;
      }

      for (int i = 0; i < steps; ++i) {
         pxScene->simulate(fixedDt);
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

   void PhysicsScene::AddRigidActor(Entity entity) {
      auto& trans = entity.Get<SceneTransformComponent>();
      auto& rb = entity.Get<RigidBodyComponent>();
      auto& geom = entity.Get<GeometryComponent>();

      PxGeometryHolder physGeom = GetPhysGeom(trans, geom);

      PxTransform physTrans = GetTransform(trans);

      PxRigidActor* actor = nullptr;
      if (rb.dynamic) {
         // todo: density
         actor = PxCreateDynamic(*gPhysics, physTrans, physGeom.any(), *gMaterial, 10.0f);
         // dynamic->setAngularDamping(0.5f);
         // dynamic->setLinearVelocity(velocity);
      } else {
         actor = PxCreateStatic(*gPhysics, physTrans, physGeom.any(), *gMaterial);
      }

      actor->userData = new Entity{entity}; // todo: use fixed allocator
      pxScene->addActor(*actor);

      rb.pxRigidActor = actor;
   }

   void PhysicsScene::RemoveRigidActor(Entity entity) {
      auto& rb = entity.Get<RigidBodyComponent>();

      pxScene->removeActor(*rb.pxRigidActor);
      delete GetEntity(rb.pxRigidActor);
      rb.pxRigidActor->userData = nullptr;

      rb.pxRigidActor = nullptr;
   }

   void PhysicsScene::OnConstructRigidBody(entt::registry& registry, entt::entity entity) {
      Entity e{ entity, &scene };
      AddRigidActor(e);
   }

   void PhysicsScene::OnDestroyRigidBody(entt::registry& registry, entt::entity entity) {
      Entity e{ entity, &scene };
      RemoveRigidActor(e);
   }

   void PhysicsScene::OnConstructTrigger(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };

      auto& trans = entity.Get<SceneTransformComponent>();
      auto& geom = entity.Get<GeometryComponent>();
      auto& trigger = entity.Get<TriggerComponent>();

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

   void PhysicsScene::OnDestroyTrigger(entt::registry& registry, entt::entity _entity) {
      Entity entity{ _entity, &scene };

      auto& trigger = entity.Get<TriggerComponent>();

      pxScene->removeActor(*trigger.pxRigidActor);
      delete GetEntity(trigger.pxRigidActor);
      trigger.pxRigidActor->userData = nullptr;

      trigger.pxRigidActor = nullptr;
   }

   // todo:
   static PxRigidActor* GetPxActor(Entity e) {
      if (!e) return nullptr;

      auto rb = e.TryGet<RigidBodyComponent>();
      return rb ? rb->pxRigidActor : nullptr;
   }

   void PhysicsScene::OnConstructDistanceJoint(entt::registry& registry, entt::entity entity) {
      Entity e{ entity, &scene };

      auto& dj = e.Get<DistanceJointComponent>();

      // todo: one of them must be dynamic

      auto actor0 = GetPxActor(dj.entity0);
      auto actor1 = GetPxActor(dj.entity0);

      if (!actor0 || !actor1) {
         return;
      }

      dj.pxDistanceJoint = PxDistanceJointCreate(*gPhysics, actor0, PxTransform{ PxIDENTITY{} }, actor1, PxTransform{ PxIDENTITY{} });
   }

   void PhysicsScene::OnDestroyDistanceJoint(entt::registry& registry, entt::entity entity) {
      // todo: destroy joint
   }

   void CreateDistanceJoint(const Entity& entity0, const Entity& entity1) {
      auto joint = PxDistanceJointCreate(*gPhysics, GetPxActor(entity0), PxTransform{ PxIDENTITY{} }, GetPxActor(entity1), PxTransform{ PxIDENTITY{} });

      joint->setMaxDistance(1.5f);
      joint->setMinDistance(1.f);

      joint->setDamping(0.5f);
      joint->setStiffness(2000.0f);

      joint->setDistanceJointFlags(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED | PxDistanceJointFlag::eMIN_DISTANCE_ENABLED | PxDistanceJointFlag::eSPRING_ENABLED);
   }
}
