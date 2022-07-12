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
      auto id = registry.create();

      auto e = Entity{ id, this };
      e.Add<UUIDComponent>();
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

      out << YAML::Key << "sceneName" << YAML::Value << "test_scene";

      // SceneTransformComponent s;
      // s.position = {1, 2, 3};
      // Typer::Get().Serialize(out, "sceneTransformComponent2", s);

      // out << YAML::Key << "sceneName" << YAML::Value << "test_scene";
      // out << YAML::Key << "sceneName2" << YAML::Value << "test_scene2";
      // out << YAML::Key << "sceneName3" << YAML::Value << "test_scene3";
      // out << YAML::Key << "sceneName4" << YAML::Value << "test_scene4";

      out << YAML::Key << "entities" << YAML::Value;

      out << YAML::BeginMap;

      for (auto [e, tag] : scene.GetEntitiesWith<TagComponent>().each()) {
         Entity entity{ e, &scene };

         const auto* name = tag.tag.data();
         out << YAML::Key << "name" << YAML::Value << name;

         out << YAML::Key << "components" << YAML::Value;

         out << YAML::BeginMap;

         // if (const auto* c = entity.TryGet<UUIDComponent>()) {
         //    Typer::Get().Serialize(out, UUIDComponent::GetName(), *c);
         // }

         if (const auto* c = entity.TryGet<SceneTransformComponent>()) {
            Typer::Get().Serialize(out, SceneTransformComponent::GetName(), *c);
         }

         out << YAML::EndMap;
      }

      out << YAML::EndMap;

      out << YAML::EndMap;

      std::ofstream fout{ path.data() };
      fout << out.c_str();
   }
}
