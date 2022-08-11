#include "pch.h"
#include "Scene.h"

#include "Component.h"
#include "Entity.h"
#include "typer/Typer.h"
#include "fs/FileSystem.h"
#include "script/NativeScript.h"

namespace pbe {

   Entity Scene::Create(std::string_view name) {
      return CreateWithUUID(UUID{}, name);
   }

   Entity Scene::CreateWithUUID(UUID uuid, std::string_view name) {
      auto id = registry.create();

      auto e = Entity{ id, this };
      e.Add<UUIDComponent>(uuid);
      if (!name.empty()) {
         e.Add<TagComponent>(name.data());
      } else {
         // todo:
         e.Add<TagComponent>(std::format("{} {}", "Entity", EntitiesCount()));
      }

      e.Add<SceneTransformComponent>();
      // e.Add<SimpleMaterialComponent>();

      return e;
   }

   Entity Scene::Duplicate(Entity entity) {
      Entity duplicatedEntity = Create(entity.Get<TagComponent>().tag);

      const auto& typer = Typer::Get();

      for (const auto ci : typer.components) {
         auto* pSrc = ci.tryGet(entity);

         if (pSrc) {
            auto* pDst = ci.getOrAdd(duplicatedEntity);
            ci.duplicate(pDst, pSrc);
         }
      }

      return duplicatedEntity;
   }

   void Scene::DestroyImmediate(Entity entity) {
      registry.destroy(entity.GetID());
   }

   void Scene::OnUpdate(float dt) {
      const auto& typer = Typer::Get();

      for (const auto& si : typer.nativeScripts) {
         si.sceneApplyFunc(*this, [dt](NativeScript& script) { script.OnUpdate(dt); });
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

   static string gAssetsPath = "../../assets/";

   string GetAssetsPath(string_view path) {
      return gAssetsPath + path.data();
   }

   void SceneSerialize(std::string_view path, Scene& scene) {
      YAML::Emitter out;

      out << YAML::BeginMap;
      {
         out << YAML::Key << "sceneName" << YAML::Value << "test_scene";

         out << YAML::Key << "entities" << YAML::Value;
         out << YAML::BeginSeq;
         {
            for (auto [e, _] : scene.GetEntitiesWith<UUIDComponent>().each()) {
               Entity entity{ e, &scene };

               out << YAML::BeginMap;
               {
                  auto uuid = (uint64)entity.Get<UUIDComponent>().uuid;

                  out << YAML::Key << "uuid" << YAML::Value << uuid;

                  if (const auto* c = entity.TryGet<TagComponent>()) {
                     out << YAML::Key << "tag" << YAML::Value << c->tag;
                  }

                  const auto& typer = Typer::Get();

                  // typer.SerializeImpl(out, SceneTransformComponent::GetName(),
                  //    GetTypeID<SceneTransformComponent>(), (byte*)&entity.Get<SceneTransformComponent>());

                  for (const auto ci : typer.components) {
                     const auto& ti = typer.GetTypeInfo(ci.typeID);

                     auto* ptr = (byte*)ci.tryGet(entity);
                     if (ptr) {
                        const char* name = ti.name.data();
                        typer.SerializeImpl(out, name, ci.typeID, ptr);
                     }
                  }
               }
               out << YAML::EndMap;
            }
         }
         out << YAML::EndSeq;
      }
      out << YAML::EndMap;

      // todo:
      // std::ofstream fout{ GetAssetsPath(path)};
      std::ofstream fout{ path.data() };
      fout << out.c_str();
   }

   Own<Scene> SceneDeserialize(std::string_view path) {
      INFO("Deserialize scene '{}'", path);

      if (!fs::exists(path)) {
         WARN("Cant find file '{}'", path);
         return {};
      }

      Own<Scene> scene{ new Scene() };

      // todo:
      // YAML::Node root = YAML::LoadFile(GetAssetsPath(path));
      YAML::Node root = YAML::LoadFile(path.data());

      auto sceneName = root["sceneName"].as<string>();

      YAML::Node entitiesNode = root["entities"];
      
      for (int i = 0; i < entitiesNode.size(); ++i) {
         auto it = entitiesNode[i];

         auto uuid = it["uuid"].as<uint64>();

         string entityTag;
         if (it["tag"]) {
            entityTag = it["tag"].as<string>();
         }

         Entity entity = scene->CreateWithUUID(UUID{ uuid }, entityTag);

         const auto& typer = Typer::Get();

         // typer.DeserializeImpl(it, SceneTransformComponent::GetName(),
         //    GetTypeID<SceneTransformComponent>(), (byte*)&entity.Get<SceneTransformComponent>());

         for (const auto ci : typer.components) {
            const auto& ti = typer.GetTypeInfo(ci.typeID);

            const char* name = ti.name.data();

            if (auto node = it[name]) {
               auto* ptr = (byte*)ci.getOrAdd(entity);
               typer.DeserializeImpl(it, name, ci.typeID, ptr);
            }
         }
      }

      return scene;
   }

}
