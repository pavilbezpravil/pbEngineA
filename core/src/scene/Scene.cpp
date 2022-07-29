#include "pch.h"
#include "Scene.h"

#include "Component.h"
#include "Entity.h"
#include "typer/Typer.h"
#include "fs/FileSystem.h"

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

   void Scene::DestroyImmediate(Entity entity) {
      registry.destroy(entity.GetID());
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
                  if (const auto* c = entity.TryGet<SceneTransformComponent>()) {
                     Typer::Get().Serialize(out, SceneTransformComponent::GetName(), *c);
                  }
                  if (const auto* c = entity.TryGet<SimpleMaterialComponent>()) {
                     Typer::Get().Serialize(out, SimpleMaterialComponent::GetName(), *c);
                  }
                  if (const auto* c = entity.TryGet<LightComponent>()) {
                     Typer::Get().Serialize(out, LightComponent::GetName(), *c);
                  }
                  if (const auto* c = entity.TryGet<DecalComponent>()) {
                     Typer::Get().Serialize(out, DecalComponent::GetName(), *c);
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

      auto name = root["sceneName"].as<string>();

      YAML::Node entitiesNode = root["entities"];
      
      for (int i = 0; i < entitiesNode.size(); ++i) {
         auto it = entitiesNode[i];

         auto uuid = it["uuid"].as<uint64>();

         string entityTag;
         if (it["tag"]) {
            entityTag = it["tag"].as<string>();
         }

         Entity entity = scene->CreateWithUUID(UUID{ uuid }, entityTag);

         if (auto node = it[SceneTransformComponent::GetName()]) {
            Typer::Get().Deserialize(it, SceneTransformComponent::GetName(), entity.Get<SceneTransformComponent>());
         }
         if (auto node = it[SimpleMaterialComponent::GetName()]) {
            Typer::Get().Deserialize(it, SimpleMaterialComponent::GetName(), entity.Add<SimpleMaterialComponent>());
         }
         if (auto node = it[LightComponent::GetName()]) {
            Typer::Get().Deserialize(it, LightComponent::GetName(), entity.Add<LightComponent>());
         }
         if (auto node = it[DecalComponent::GetName()]) {
            Typer::Get().Deserialize(it, DecalComponent::GetName(), entity.Add<DecalComponent>());
         }
      }

      return scene;
   }

}
