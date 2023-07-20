#include "pch.h"
#include "Scene.h"

#include "Component.h"
#include "Entity.h"
#include "typer/Typer.h"
#include "fs/FileSystem.h"
#include "rend/DbgRend.h"
#include "script/Script.h"
#include "typer/Serialize.h"
#include "physics/PhysicsScene.h"

namespace pbe {

   Scene::Scene(bool withRoot) {
      dbgRend = std::make_unique<DbgRend>();

      if (withRoot) {
         SetRootEntity(CreateWithUUID(UUID{}, Entity{}, "Scene"));
      }

      pPhysics = std::make_unique<PhysicsScene>(*this); // todo: for all scene is it needed?

      // todo:
      registry.on_construct<RigidBodyComponent>().connect<&PhysicsScene::OnConstructRigidBody>(pPhysics);
      registry.on_destroy<RigidBodyComponent>().connect<&PhysicsScene::OnDestroyRigidBody>(pPhysics);

      registry.on_construct<DistanceJointComponent>().connect<&PhysicsScene::OnConstructDistanceJoint>(pPhysics);
      registry.on_destroy<DistanceJointComponent>().connect<&PhysicsScene::OnDestroyDistanceJoint>(pPhysics);
   }

   Scene::~Scene() {
      // todo: destroy root entity
      registry.clear(); // registry doesn't have call destructor for components without direct call clear
   }

   Entity Scene::Create(std::string_view name) {
      return CreateWithUUID(UUID{}, GetRootEntity(), name);
   }

   Entity Scene::Create(const Entity& parent, std::string_view name) {
      return CreateWithUUID(UUID{}, parent ? parent : GetRootEntity(), name);
   }

   Entity Scene::CreateWithUUID(UUID uuid, const Entity& parent, std::string_view name) {
      auto entityID = registry.create();

      auto entity = Entity{ entityID, this };
      entity.Add<UUIDComponent>(uuid);
      if (!name.empty()) {
         entity.Add<TagComponent>(name.data());
      } else {
         // todo: support entity without name
         entity.Add<TagComponent>(std::format("{} {}", "Entity", EntitiesCount()));
      }

      entity.Add<SceneTransformComponent>(entity, parent);

      ASSERT(uuidToEntities.find(uuid) == uuidToEntities.end());
      uuidToEntities[uuid] = entityID;

      return entity;
   }

   Entity Scene::GetEntity(UUID uuid) {
      auto it = uuidToEntities.find(uuid);
      return it == uuidToEntities.end() ? Entity{} : Entity{ uuidToEntities[(uint64)uuid], this };
   }

   Entity Scene::GetRootEntity() {
      return Entity{ rootEntityId, this };
   }

   void Scene::SetRootEntity(const Entity& entity) {
      ASSERT(entity.GetTransform().HasParent() == false);
      ASSERT(rootEntityId == entt::null);
      rootEntityId = entity.GetID();
   }

   void Scene::Duplicate(Entity& dst, const Entity& src, bool copyUUID) {
      ASSERT(!copyUUID || dst.GetScene() != src.GetScene());

      auto& srcTrans = src.GetTransform();

      // todo:
      auto& dstTrans = dst.GetTransform();
      dstTrans.position = srcTrans.position;
      dstTrans.rotation = srcTrans.rotation;
      dstTrans.scale = srcTrans.scale;

      for (auto child : srcTrans.children) {
         Entity duplicatedChild = CreateWithUUID(copyUUID ? child.GetUUID() : UUID{}, dst, child.GetName());
         Duplicate(duplicatedChild, child, copyUUID);
      }

      const auto& typer = Typer::Get();

      for (const auto& ci : typer.components) {
         auto* pSrc = ci.tryGetConst(src);

         if (pSrc) {
            ci.copyCtor(dst, pSrc);

            // todo:
            // auto* pDst = ci.tryGet(dst);
            // if (pDst) {
            //    // scene trasform component and tag already exist
            //    // think how to remove this branch
            //    ci.duplicate(pDst, pSrc);
            // } else {
            //    ci.copyCtor(dst, pSrc);
            // }
         }
      }
   }

   Entity Scene::Duplicate(const Entity& entity) {
      // todo: dirty code
      auto name = entity.GetName();
      int nameLen = (int)strlen(name);

      int iter = nameLen - 1;
      while (iter >= 0 && (std::isdigit(name[iter]) || name[iter] == ' ')) {
         iter--;
      }

      int idx = iter + 1 != nameLen ? atoi(name + iter + 1) : 0;
      string_view namePrefix = string_view(name, iter + 1);

      Entity duplicatedEntity = Create(entity.GetTransform().parent,
         std::format("{} {}", namePrefix, ++idx));
      Duplicate(duplicatedEntity, entity, false);
      return duplicatedEntity;
   }

   void Scene::DestroyImmediate(Entity entity, bool withChilds) {
      auto& trans = entity.GetTransform();

      // todo: mb use iterator on children?
      for (int iChild = (int)trans.children.size() - 1; iChild >= 0; --iChild) {
         auto& child = trans.children[iChild];
         if (withChilds) {
            DestroyImmediate(child, true);
         } else {
            child.GetTransform().SetParent(trans.parent);
         }

         // todo: scene component may be shrink during child destroy
         trans = entity.GetTransform();
      }
      trans.SetParent();

      uuidToEntities.erase(entity.GetUUID());
      registry.destroy(entity.GetID());
   }

   void Scene::OnStart() {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.scripts) {
         si.initialize(*this);
      }

      for (const auto& si : typer.scripts) {
         si.sceneApplyFunc(*this, [](Script& script) { script.OnEnable(); });
      }
   }

   void Scene::OnUpdate(float dt) {
      if (pPhysics) {
         pPhysics->Simulation(dt);
      }

      const auto& typer = Typer::Get();

      for (const auto& si : typer.scripts) {
         si.sceneApplyFunc(*this, [dt](Script& script) { script.OnUpdate(dt); });
      }
   }

   void Scene::OnStop() {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.scripts) {
         si.sceneApplyFunc(*this, [](Script& script) { script.OnDisable(); });
      }
   }

   Entity Scene::FindByName(std::string_view name) {
      for (auto [e, tag] : View<TagComponent>().each()) {
         if (tag.tag == name) {
            return Entity{e, this};
         }
      }
      return {};
   }

   uint Scene::EntitiesCount() const {
      return (uint)uuidToEntities.size();
   }

   Own<Scene> Scene::Copy() const {
      auto pScene = std::make_unique<Scene>(false);

      Entity srcRoot{ rootEntityId, const_cast<Scene*>(this) };
      Entity dstRoot = pScene->CreateWithUUID(srcRoot.GetUUID(), Entity{}, srcRoot.GetName());
      pScene->SetRootEntity(dstRoot);
      pScene->Duplicate(dstRoot, srcRoot, true);

      return pScene;
   }

   static Scene* gCurrentDeserializedScene = nullptr;

   Scene* Scene::GetCurrentDeserializedScene() {
      return gCurrentDeserializedScene;
   }

   static string gAssetsPath = "../../assets/";

   string GetAssetsPath(string_view path) {
      return gAssetsPath + path.data();
   }

   void SceneSerialize(std::string_view path, Scene& scene) {
      Serializer ser;

      {
         SERIALIZER_MAP(ser);
         {
            // ser.KeyValue("sceneName", "test_scene");

            ser.Key("entities");
            SERIALIZER_SEQ(ser);
            {
               std::vector<uint64> entitiesUuids;

               for (auto [_, uuid] : scene.View<UUIDComponent>().each()) {
                  entitiesUuids.emplace_back((uint64)uuid.uuid);
               }

               std::ranges::sort(entitiesUuids);

               for (auto uuid : entitiesUuids) {
                  Entity entity = scene.GetEntity(uuid);
                  EntitySerialize(ser, entity);
               }
            }
         }
      }

      // todo:
      // GetAssetsPath(path)
      ser.SaveToFile(path);
   }

   Own<Scene> SceneDeserialize(std::string_view path) {
      INFO("Deserialize scene '{}'", path);

      if (!fs::exists(path)) {
         WARN("Cant find file '{}'", path);
         return {};
      }

      Own<Scene> scene = std::make_unique<Scene>(false);

      gCurrentDeserializedScene = scene.get();

      // todo:
      // GetAssetsPath(path);
      auto deser = Deserializer::FromFile(path);

      // auto sceneName = deser["sceneName"].As<string>();

      auto entitiesNode = deser["entities"];

      // on first iteration create all entities
      for (int i = 0; i < entitiesNode.Size(); ++i) {
         auto it = entitiesNode[i];

         auto uuid = it["uuid"].As<uint64>();

         string entityTag;
         if (it["tag"]) {
            entityTag = it["tag"].As<string>();
         }

         scene->CreateWithUUID(UUID{ uuid }, Entity{}, entityTag);
      }

      // on second iteration create all components
      for (int i = 0; i < entitiesNode.Size(); ++i) {
         auto it = entitiesNode[i];
         EntityDeserialize(it, *scene);
      }

      entt::entity rootEntityId = entt::null;
      for (auto [e, trans] : scene->View<SceneTransformComponent>().each()) {
         if (trans.parent) {
            continue;
         }

         if (rootEntityId != entt::null) {
            WARN("Scene has several roots. Create new root and parenting to it all entities without parent");
            auto newRoot = scene->CreateWithUUID(UUID{}, Entity{}, "Scene");
            rootEntityId = newRoot.GetID();

            for (auto [e, trans] : scene->View<SceneTransformComponent>().each()) {
               if (!trans.parent) {
                  trans.SetParent(newRoot);
               }
            }

            break;
         }

         rootEntityId = e;
         // todo: remove after all scenes will have root entity
         // break;
      }

      scene->SetRootEntity(Entity{ rootEntityId, scene.get() });

      gCurrentDeserializedScene = nullptr;

      return scene;
   }

   void EntitySerialize(Serializer& ser, const Entity& entity) {
      SERIALIZER_MAP(ser);
      {
         auto uuid = (uint64)entity.Get<UUIDComponent>().uuid;

         ser.KeyValue("uuid", uuid);

         if (const auto* c = entity.TryGet<TagComponent>()) {
            ser.KeyValue("tag", c->tag);
         }

         ser.Ser("SceneTransformComponent", entity.GetTransform());

         const auto& typer = Typer::Get();

         for (const auto& ci : typer.components) {
            const auto& ti = typer.GetTypeInfo(ci.typeID);

            auto* ptr = (byte*)ci.tryGetConst(entity);
            if (ptr) {
               const char* name = ti.name.data();
               ser.Ser(name, ci.typeID, ptr);
            }
         }
      }
   }

   void EntityDeserialize(const Deserializer& deser, Scene& scene) {
      auto uuid = deser["uuid"].As<uint64>();

      Entity entity = scene.GetEntity(uuid);
      if (entity) {
         entity.RemoveAll<UUIDComponent, TagComponent, SceneTransformComponent>();
      } else {
         ASSERT(false);
         entity = scene.CreateWithUUID(uuid, Entity{});
      }

      string name = std::invoke([&] {
         if (deser["tag"]) {
            return deser["tag"].As<string>();
         }
         return string{};
      });

      entity.GetOrAdd<TagComponent>().tag = name;

      deser.Deser("SceneTransformComponent", entity.GetTransform());

      const auto& typer = Typer::Get();

      // std::array<byte, 128> data; // todo:

      for (const auto& ci : typer.components) {
         const auto& ti = typer.GetTypeInfo(ci.typeID);

         // ASSERT(ti.typeSizeOf < data.size());

         const char* name = ti.name.data();

         if (auto node = deser[name]) {
            // todo: use move ctor
            auto* ptr = (byte*)ci.getOrAdd(entity);
            deser.Deser(name, ci.typeID, ptr);
         }
      }
   }
}
