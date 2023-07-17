#include "pch.h"
#include "PhysicsScene.h"
#include "PhysXTypeConvet.h"
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
      gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.6f);

      // PxRigidStatic* groundPlane = PxCreatePlane(*gPhysics, PxPlane(0, 1, 0, 0), *gMaterial);
      // gScene->addActor(*groundPlane);

      // for (PxU32 i = 0; i < 5; i++)
      //    createStack(PxTransform(PxVec3(0, 0, stackZ -= 10.0f)), 10, 2.0f);
      //
      // if (!interactive)
      //    createDynamic(PxTransform(PxVec3(0, 40, 100)), PxSphereGeometry(10), PxVec3(0, -50, -100));
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

   PhysicsScene::PhysicsScene(Scene& scene) : scene(scene) {
      PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
      sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
      sceneDesc.cpuDispatcher = gDispatcher;
      sceneDesc.filterShader = PxDefaultSimulationFilterShader;
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
         scene.GetEntitiesWith<SceneTransformComponent, RigidBodyComponent>().each()) {
         PxTransform pxTrans{ Vec3ToPx(trans.Position()), QuatToPx(trans.Rotation()) };
         rb.pxRigidActor->setGlobalPose(pxTrans);
      }
   }

   void PhysicsScene::Simulation(float dt) {
      SyncPhysicsWithScene();

      pxScene->simulate(dt); // todO: fixed time step
      pxScene->fetchResults(true);

      UpdateSceneAfterPhysics(); // todo:
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

      PxTransform t{ Vec3ToPx(trans.Position()), QuatToPx(trans.Rotation()) };

      auto& rb = entity.Get<RigidBodyComponent>();

      auto& geom = entity.Get<GeometryComponent>();

      // todo: think about tran.scale

      PxGeometryHolder physGeom;
      if (geom.type == GeomType::Sphere) {
         physGeom = PxSphereGeometry(geom.sizeData.x * trans.Scale().x); // todo: scale, default radius == 1 -> diameter == 2 (it is bad)
      } else if (geom.type == GeomType::Box) {
         physGeom = PxBoxGeometry(Vec3ToPx(geom.sizeData * trans.Scale() / 2.f));
      }

      PxRigidActor* actor = nullptr;
      if (rb.dynamic) {
         // todo: density
         actor = PxCreateDynamic(*gPhysics, t, physGeom.any(), *gMaterial, 10.0f);
         // dynamic->setAngularDamping(0.5f);
         // dynamic->setLinearVelocity(velocity);
      } else {
         actor = PxCreateStatic(*gPhysics, t, physGeom.any(), *gMaterial);
      }

      actor->userData = new Entity{entity}; // todo: use fixed allocator

      pxScene->addActor(*actor);

      rb.pxRigidActor = actor;
   }

   void PhysicsScene::RemoveRigidActor(Entity entity) {
      auto& rb = entity.Get<RigidBodyComponent>();

      pxScene->removeActor(*rb.pxRigidActor);
      delete (Entity*)rb.pxRigidActor->userData;

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
}
