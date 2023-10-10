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

      // todo: for all scene is it needed?
      AddSystem(std::make_unique<PhysicsScene>(*this));
   }

   Scene::~Scene() {
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
      //

      Entity duplicatedEntity = Create(std::format("{} {}", namePrefix, ++idx));

      auto& trans = entity.GetTransform();
      duplicatedEntity.GetTransform().SetParent(trans.parent, trans.GetChildIdx() + 1);

      std::unordered_map<UUID, DuplicateContext> hierEntitiesMap;
      DuplicateHierEntitiesWithMap(duplicatedEntity, entity, false, hierEntitiesMap);

      Duplicate(duplicatedEntity, entity, false, hierEntitiesMap);

      DuplicateEntityEnable(duplicatedEntity, hierEntitiesMap);
      ProcessDelayedEnable();

      return duplicatedEntity;
   }

   bool Scene::EntityEnabled(const Entity& entity) const {
      return !entity.HasAny<DisableMarker, DelayedDisableMarker, DelayedEnableMarker>();
   }

   void Scene::EntityEnable(Entity& entity, bool withChilds) {
      if (entity.Has<DelayedEnableMarker>()) {
         return;
      }
      if (entity.Has<DelayedDisableMarker>()) {
         entity.Remove<DelayedDisableMarker>();
         return;
      }
      if (!entity.Has<DisableMarker>()) {
         return;
      }

      if (withChilds) {
         for (auto& child : entity.GetTransform().children) {
            EntityEnable(child);
         }
      }

      entity.Add<DelayedEnableMarker>();
      ASSERT(entity.Has<DisableMarker>() && !entity.Has<DelayedDisableMarker>());
   }

   // todo: when add child to disabled entity, it must be disabled too
   void Scene::EntityDisable(Entity& entity, bool withChilds) {
      if (entity.HasAny<DisableMarker, DelayedDisableMarker>()) {
         return;
      }
      if (entity.Has<DelayedEnableMarker>()) {
         entity.Remove<DelayedEnableMarker>();
         return;
      }

      if (withChilds) {
         for (auto& child : entity.GetTransform().children) {
            EntityDisable(child);
         }
      }

      entity.Add<DelayedDisableMarker>();
      ASSERT(!(entity.HasAny<DisableMarker, DelayedEnableMarker>()));
   }

   void Scene::ProcessDelayedEnable() {
      // disable
      for (auto& system : systems) {
         system->OnEntityDisable();
      }

      // todo: iterate over all systems or only scripts
      // const auto& typer = Typer::Get();
      // for (const auto& si : typer.scripts) {
      //    si.sceneApplyFunc(*this, [](Script& script) { script.OnDisable(); });
      // }

      for (auto e : ViewAll<DelayedDisableMarker>()) {
         registry.emplace<DisableMarker>(e);
      }
      registry.clear<DelayedDisableMarker>();

      // enable
      for (auto& system : systems) {
         system->OnEntityEnable();
      }

      // todo: iterate over all systems or only scripts
      // for (const auto& si : typer.scripts) {
      //    si.sceneApplyFunc(*this, [](Script& script) { script.OnEnable(); });
      // }

      for (auto [entityID, trans] : ViewAll<DelayedEnableMarker, SceneTransformComponent>().each()) {
         trans.UpdatePrevTransform();
         registry.erase<DisableMarker>(entityID);
      }
      registry.clear<DelayedEnableMarker>();
   }

   struct DelayedDestroyMarker {
      bool withChilds = false; // todo: remove?
   };

   void Scene::DestroyDelayed(Entity entity, bool withChilds) {
      entity.GetOrAdd<DelayedDestroyMarker>(withChilds);
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
      trans.SetParentInternal();

      uuidToEntities.erase(entity.GetUUID());
      registry.destroy(entity.GetID());
   }

   void Scene::DestroyDelayedEntities() {
      for (auto [e, marker] : registry.view<DelayedDestroyMarker>().each()) {
         DestroyImmediate(Entity{e, this}, marker.withChilds);
      }
      registry.clear<DelayedDestroyMarker>();
   }

   void Scene::OnSync() {
      DestroyDelayedEntities();
      ProcessDelayedEnable();
   }

   void Scene::OnTick() {
      OnSync();

      // sync phys scene with changed transforms outside physics
      GetPhysics()->SyncPhysicsWithScene();
      ClearComponent<TransformChangedMarker>();

      for (auto [entityID, trans] : View<SceneTransformComponent>().each()) {
         trans.UpdatePrevTransform();
      }
   }

   void Scene::OnStart() {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.scripts) {
         si.sceneApplyFunc(*this, [](Script& script) { script.OnEnable(); });
      }
   }

   void Scene::OnUpdate(float dt) {
      for (auto [entityID, td] : View<TimedDieComponent>().each()) {
         td.time -= dt;
         if (td.time < 0.f) {
            DestroyDelayed(Entity{ entityID, this });
         }
      }
      OnSync();

      for (auto& system : systems) {
         system->OnUpdate(dt);
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

   void Scene::AddSystem(Own<System>&& system) {
      system->SetScene(this);
      system->OnSetEventHandlers(registry);
      systems.emplace_back(std::move(system));
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

      std::unordered_map<UUID, DuplicateContext> hierEntitiesMap;
      pScene->DuplicateHierEntitiesWithMap(dstRoot, srcRoot, true, hierEntitiesMap);

      pScene->Duplicate(dstRoot, srcRoot, true, hierEntitiesMap);

      pScene->DuplicateEntityEnable(dstRoot, hierEntitiesMap);
      pScene->ProcessDelayedEnable();

      return pScene;
   }

   static Scene* gCurrentDeserializedScene = nullptr;

   Scene* Scene::GetCurrentDeserializedScene() {
      return gCurrentDeserializedScene;
   }

   void Scene::EntityDisableImmediate(Entity& entity) {
      ASSERT(!(entity.HasAny<DisableMarker, DelayedEnableMarker, DelayedDisableMarker>()));
      entity.Add<DisableMarker>();
   }

   void Scene::DuplicateHierEntitiesWithMap(Entity& dst, const Entity& src, bool copyUUID, std::unordered_map<UUID, DuplicateContext>& hierEntitiesMap) {
      hierEntitiesMap[src.GetUUID()] = DuplicateContext{ dst.GetID(), src.Enabled() };
      // while duplicate entities, disable them
      dst.GetScene()->EntityDisableImmediate(dst);

      auto& srcTrans = src.GetTransform();

      for (auto child : srcTrans.children) {
         Entity duplicatedChild = CreateWithUUID(copyUUID ? child.GetUUID() : UUID{}, dst, child.GetName());
         DuplicateHierEntitiesWithMap(duplicatedChild, child, copyUUID, hierEntitiesMap);
      }
   }

   void Scene::Duplicate(Entity& dst, const Entity& src, bool copyUUID, std::unordered_map<UUID, DuplicateContext>& hierEntitiesMap) {
      // if copyUUID == true, dst must be in another scene
      ASSERT(!copyUUID || dst.GetScene() != src.GetScene());

      auto& srcTrans = src.GetTransform();

      // todo: move after loop?
      auto& dstTrans = dst.GetTransform();
      dstTrans.Local() = srcTrans.Local();
      dstTrans.UpdatePrevTransform();

      for (auto child : srcTrans.children) {
         Entity duplicatedChild = { hierEntitiesMap[child.GetUUID()].enttEntity, dst.GetScene()};
         Duplicate(duplicatedChild, child, copyUUID, hierEntitiesMap);
      }

      const auto& typer = Typer::Get();

      // todo: use entt for iterate components
      for (const auto& ci : typer.components) {
         auto* pSrc = ci.tryGetConst(src);
         if (!pSrc) {
            continue;
         }

         auto pDst = ci.copyCtor(dst, pSrc);

         const auto& ti = typer.GetTypeInfo(ci.typeID);
         if (!ti.hasEntityRef) {
            continue;
         }

         for (const auto& field : ti.fields) {
            auto& filedTypeInfo = typer.GetTypeInfo(field.typeID);

            if (filedTypeInfo.hasEntityRef) {
               // todo: while dont support nested entity ref
               ASSERT(filedTypeInfo.IsSimpleType());

               // it like from src, because it was copied by value in 'copyCtor'
               auto pDstEntity = (Entity*)((byte*)pDst + field.offset);
               if (!*pDstEntity) {
                  continue;
               }

               auto uuid = pDstEntity->GetUUID();
               
               auto it = hierEntitiesMap.find(uuid);
               if (it != hierEntitiesMap.end()) {
                  *pDstEntity = Entity{ it->second.enttEntity, dst.GetScene()};
               }
            }
         }
      }
   }

   void Scene::DuplicateEntityEnable(Entity& root, std::unordered_map<UUID, DuplicateContext>& hierEntitiesMap) {
      ASSERT(root.GetScene() == this);
      // todo: mb create set with enabled entities?
      // todo: not fastest solution, find better way to implement copy scene, duplicate logic

      for (auto [_, context] : hierEntitiesMap) {
         Entity entity{ context.enttEntity, this };
         if (context.enabled) {
            EntityEnable(entity, false);
         } else {
            EntityDisable(entity, false);
         }
      }
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

               for (auto [_, uuid] : scene.ViewAll<UUIDComponent>().each()) {
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

         Entity entity = scene->CreateWithUUID(UUID{ uuid }, Entity{}, entityTag);
         scene->EntityDisableImmediate(entity);
      }

      // on second iteration create all components
      for (int i = 0; i < entitiesNode.Size(); ++i) {
         auto it = entitiesNode[i];
         EntityDeserialize(it, *scene);
      }

      entt::entity rootEntityId = entt::null;
      for (auto [e, trans] : scene->ViewAll<SceneTransformComponent>().each()) {
         if (trans.parent) {
            continue;
         }

         if (rootEntityId != entt::null) {
            WARN("Scene has several roots. Create new root and parenting to it all entities without parent");
            auto newRoot = scene->CreateWithUUID(UUID{}, Entity{}, "Scene");
            rootEntityId = newRoot.GetID();

            for (auto [e, trans] : scene->ViewAll<SceneTransformComponent>().each()) {
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

      Entity rootEntity = { rootEntityId, scene.get() };
      scene->SetRootEntity(rootEntity);

      scene->ProcessDelayedEnable();

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

         if (!entity.Enabled()) {
            // todo: without value. As I understend, yaml doesnt support key without value
            ser.KeyValue("disabled", true);
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
         // todo: нахуй ты это написал, ебаный ишак?
         // entity.RemoveAll<UUIDComponent, TagComponent, SceneTransformComponent>();
      } else {
         ASSERT(false);
         entity = scene.CreateWithUUID(uuid, Entity{});
      }

      ASSERT(!entity.Enabled());

      string name = std::invoke([&] {
         if (deser["tag"]) {
            return deser["tag"].As<string>();
         }
         return string{};
      });

      entity.GetOrAdd<TagComponent>().tag = name;

      bool enabled = !deser["disabled"];

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

      if (enabled) {
         entity.GetScene()->EntityEnable(entity, false);
      }
   }
}
