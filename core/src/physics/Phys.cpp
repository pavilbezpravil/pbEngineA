#include "pch.h"
#include "Phys.h"

#include "scene/Entity.h"
#include "PhysUtils.h"
#include "core/Log.h"

#include "NvBlastTk.h"


using namespace Nv::Blast;

namespace pbe {

#define PVD_HOST "127.0.0.1"	//Set this to the IP address of the system running the PhysX Visual Debugger that you want to connect to.

   static PxDefaultAllocator gAllocator;
   static PxDefaultErrorCallback	gErrorCallback;
   static PxFoundation* gFoundation = NULL;
   static PxPhysics* gPhysics = NULL;
   static PxDefaultCpuDispatcher* gDispatcher = NULL;
   static PxMaterial* gMaterial = NULL;
   static PxPvd* gPvd = NULL;

   TkFramework* tkFramework = NULL;

   void InitPhysics() {
      gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);

      gPvd = PxCreatePvd(*gFoundation);
      PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
      gPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

      gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true, gPvd);
      gDispatcher = PxDefaultCpuDispatcherCreate(2);
      gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.25f);

      tkFramework = NvBlastTkFrameworkCreate();
   }

   void TermPhysics() {
      tkFramework->release();

      PX_RELEASE(gDispatcher);
      PX_RELEASE(gPhysics);
      if (gPvd) {
         PxPvdTransport* transport = gPvd->getTransport();
         gPvd->release();	gPvd = NULL;
         PX_RELEASE(transport);
      }
      PX_RELEASE(gFoundation);
   }

   PxPhysics* GetPxPhysics() {
      return gPhysics;
   }

   PxCpuDispatcher* GetPxCpuDispatcher() {
      return gDispatcher;
   }

   PxMaterial* GetPxMaterial() {
      return gMaterial;
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

   SimulationEventCallback::SimulationEventCallback(PhysicsScene* physScene) : physScene(physScene) {}

   void SimulationEventCallback::onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) {}

   void SimulationEventCallback::onWake(PxActor** actors, PxU32 count) {
      for (PxU32 i = 0; i < count; i++) {
         Entity& e = *GetEntity(actors[i]);
         INFO("Wake event {}", e.GetName());
      }
   }

   void SimulationEventCallback::onSleep(PxActor** actors, PxU32 count) {
      for (PxU32 i = 0; i < count; i++) {
         Entity& e = *GetEntity(actors[i]);
         INFO("Sleep event {}", e.GetName());
      }
   }

   void SimulationEventCallback::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) {
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

   void SimulationEventCallback::onTrigger(PxTriggerPair* pairs, PxU32 count) {
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
   void SimulationEventCallback::onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) {}

}
