#include "pch.h"
#include "Scene.h"

#include "Component.h"
#include "Entity.h"
#include "typer/Typer.h"
#include "fs/FileSystem.h"
#include "rend/DbgRend.h"
#include "script/NativeScript.h"
#include "typer/Serialize.h"

namespace pbe {

   Scene::Scene() {
      dbgRend = std::make_unique<DbgRend>();
      // rootEntityId = Create("Scene").GetID();

      // todo:
      // RemoveAllChild();
      // SetParent();
      // registry.on_construct<SceneTransformComponent>().connect<&Scene::OnUUIDComponentAdded>(this);
   }

   Scene::~Scene() {}

   Entity Scene::Create(std::string_view name) {
      return CreateWithUUID(UUID{}, name);
   }

   Entity Scene::CreateWithUUID(UUID uuid, std::string_view name) {
      auto entityID = registry.create();

      auto e = Entity{ entityID, this };
      e.Add<UUIDComponent>(uuid);
      if (!name.empty()) {
         e.Add<TagComponent>(name.data());
      } else {
         // todo:
         e.Add<TagComponent>(std::format("{} {}", "Entity", EntitiesCount()));
      }

      e.Add<SceneTransformComponent>().entity = e;

      ASSERT(uuidToEntities.find(uuid) == uuidToEntities.end());
      uuidToEntities[uuid] = entityID;

      return e;
   }

   Entity Scene::GetEntity(UUID uuid) {
      auto it = uuidToEntities.find(uuid);
      return it == uuidToEntities.end() ? Entity{} : Entity{ uuidToEntities[(uint64)uuid], this };
   }

   Entity Scene::GetRootEntity() {
      return Entity{ rootEntityId, this };
   }

   void Scene::Duplicate(Entity dst, Entity src) {
      const auto& typer = Typer::Get();

      for (const auto ci : typer.components) {
         auto* pSrc = ci.tryGet(src);

         if (pSrc) {
            auto* pDst = ci.getOrAdd(dst);
            ci.duplicate(pDst, pSrc);
         }
      }
   }

   Entity Scene::Duplicate(Entity entity) {
      Entity duplicatedEntity = Create(entity.Get<TagComponent>().tag + " copy");
      Duplicate(duplicatedEntity, entity);
      return duplicatedEntity;
   }

   void Scene::DestroyImmediate(Entity entity) {
      // todo:
      entity.Get<SceneTransformComponent>().RemoveAllChild();

      uuidToEntities.erase(entity.GetUUID());
      registry.destroy(entity.GetID());
   }

   void Scene::OnStart() {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.nativeScripts) {
         si.initialize(*this);
      }

      for (const auto& si : typer.nativeScripts) {
         si.sceneApplyFunc(*this, [](NativeScript& script) { script.OnEnable(); });
      }
   }

   void Scene::OnUpdate(float dt) {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.nativeScripts) {
         si.sceneApplyFunc(*this, [dt](NativeScript& script) { script.OnUpdate(dt); });
      }
   }

   void Scene::OnStop() {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.nativeScripts) {
         si.sceneApplyFunc(*this, [](NativeScript& script) { script.OnDisable(); });
      }
   }

   Entity Scene::FindByName(std::string_view name) {
      for (auto [e, tag] : GetEntitiesWith<TagComponent>().each()) {
         if (tag.tag == name) {
            return Entity{e, this};
         }
      }
      return {};
   }

   uint Scene::EntitiesCount() const {
      return (uint)registry.alive();
   }

   Own<Scene> Scene::Copy() {
      auto pScene = std::make_unique<Scene>();

      for (auto [e, uuid] : GetEntitiesWith<UUIDComponent>().each()) {
         Entity src{e, this};
         Entity dst = pScene->CreateWithUUID(uuid.uuid, registry.get<TagComponent>(e).tag);
         Duplicate(dst, src);
      }

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
            ser.KeyValue("sceneName", "test_scene");

            ser.Key("entities");
            SERIALIZER_SEQ(ser);
            {
               std::vector<uint64> entitiesUuids;

               for (auto [_, uuid] : scene.GetEntitiesWith<UUIDComponent>().each()) {
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

      Own<Scene> scene{ new Scene() };

      gCurrentDeserializedScene = scene.get();

      // todo:
      // GetAssetsPath(path);
      auto deser = Deserializer::FromFile(path);

      auto sceneName = deser["sceneName"].As<string>();

      auto entitiesNode = deser["entities"];

      // on first iteration create all entities
      for (int i = 0; i < entitiesNode.Size(); ++i) {
         auto it = entitiesNode[i];

         auto uuid = it["uuid"].As<uint64>();

         string entityTag;
         if (it["tag"]) {
            entityTag = it["tag"].As<string>();
         }

         scene->CreateWithUUID(UUID{ uuid }, entityTag);
      }

      // on second iteration create all components
      for (int i = 0; i < entitiesNode.Size(); ++i) {
         auto it = entitiesNode[i];
         EntityDeserialize(it, *scene);
      }

      gCurrentDeserializedScene = nullptr;

      return scene;
   }

   void EntitySerialize(Serializer& ser, Entity& entity) {
      SERIALIZER_MAP(ser);
      {
         auto uuid = (uint64)entity.Get<UUIDComponent>().uuid;

         ser.KeyValue("uuid", uuid);

         if (const auto* c = entity.TryGet<TagComponent>()) {
            ser.KeyValue("tag", c->tag);
         }

         const auto& typer = Typer::Get();

         for (const auto ci : typer.components) {
            const auto& ti = typer.GetTypeInfo(ci.typeID);

            auto* ptr = (byte*)ci.tryGet(entity);
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
         entity = scene.CreateWithUUID(uuid);
      }

      string name = std::invoke([&] {
         if (deser["tag"]) {
            return deser["tag"].As<string>();
         }
         return string{};
      });

      entity.GetOrAdd<TagComponent>().tag = name;

      const auto& typer = Typer::Get();

      for (const auto ci : typer.components) {
         const auto& ti = typer.GetTypeInfo(ci.typeID);

         const char* name = ti.name.data();

         if (auto node = deser[name]) {
            auto* ptr = (byte*)ci.getOrAdd(entity);
            deser.Deser(name, ci.typeID, ptr);
         }
      }
   }
}
