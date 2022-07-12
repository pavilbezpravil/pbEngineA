#include "Scene.h"

#include <fstream>
#include "Component.h"
#include "Entity.h"
#include "typer/Typer.h"

// todo:
#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

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
      }

      e.Add<SceneTransformComponent>();
      e.Add<TestCustomUIComponent>();

      return e;
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
               }
               out << YAML::EndMap;
            }
         }
         out << YAML::EndSeq;
      }
      out << YAML::EndMap;

      std::ofstream fout{ path.data() };
      fout << out.c_str();
   }

   Own<Scene> SceneDeserialize(std::string_view path) {
      INFO("Deserialize scene '{}'", path);

      Own<Scene> scene{ new Scene() };

      YAML::Node root = YAML::LoadFile(path.data());

      auto name = root["sceneName"].as<string>();
      INFO("sceneName {}", name);

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

      }

      return scene;
   }

}
