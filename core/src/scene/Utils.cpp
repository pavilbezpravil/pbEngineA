#include "pch.h"
#include "Utils.h"

#include "Component.h"
#include "../../../pbeEditor/src/EditorSelection.h"
#include "gui/Gui.h"
#include "math/Random.h"
#include "physics/PhysComponents.h"
#include "physics/PhysicsScene.h"

namespace pbe {

   Entity CreateEmpty(Scene& scene, string_view namePrefix, Entity parent, const vec3& pos) {
      // todo: find appropriate name
      auto entity = scene.Create(parent, namePrefix);
      entity.Get<SceneTransformComponent>().position = pos;
      return entity;
   }

   Entity CreateCube(Scene& scene, const CubeDesc& desc) {
      auto entity = CreateEmpty(scene, desc.namePrefix, desc.parent, desc.pos);

      auto& trans = entity.Get<SceneTransformComponent>();
      trans.scale = desc.scale;
      trans.rotation = desc.rotation;

      entity.Add<GeometryComponent>();
      entity.Add<MaterialComponent>().baseColor = desc.color;

      // todo:
      RigidBodyComponent _rb{};
      _rb.dynamic = desc.dynamic;
      entity.Add<RigidBodyComponent>(_rb);

      return entity;
   }

   Entity CreateDirectLight(Scene& scene, string_view namePrefix, const vec3& pos) {
      auto entity = CreateEmpty(scene, namePrefix, {}, pos);
      entity.Get<SceneTransformComponent>().rotation = quat{ vec3{PIHalf * 0.5, PIHalf * 0.5, 0 } };
      entity.Add<DirectLightComponent>();
      return entity;
   }

   Entity CreateSky(Scene& scene, string_view namePrefix, const vec3& pos) {
      auto entity = CreateEmpty(scene, namePrefix, {}, pos);
      entity.Add<SkyComponent>();
      return entity;
   }

   Entity CreateTrigger(Scene& scene, const vec3& pos) {
      auto entity = CreateEmpty(scene, "Trigger", {}, pos);
      entity.Add<GeometryComponent>();
      entity.Add<TriggerComponent>();
      return entity;
   }

   Entity SceneAddEntityMenu(Scene& scene, const vec3& spawnPosHint, EditorSelection* selection) {
      if (ImGui::MenuItem("Create Empty")) {
         return CreateEmpty(scene);
      }

      // if (UI_MENU("Geometry")) {
      //    // todo: add just geom without physics
      // }

      if (UI_MENU("Physics")) {
         if (ImGui::MenuItem("Dynamic Cube")) {
            return CreateCube(scene, CubeDesc{ .pos = spawnPosHint, .dynamic = true });
         }
         if (ImGui::MenuItem("Static Cube")) {
            return CreateCube(scene, CubeDesc{ .pos = spawnPosHint, .dynamic = false });
         }
         if (ImGui::MenuItem("Trigger")) {
            return CreateTrigger(scene, spawnPosHint);
         }

         bool jointEnabled = selection && selection->selected.size() == 2;
         if (UI_MENU("Joints", jointEnabled)) {
            Entity parent = selection->selected[0];

            JointComponent joint{};
            joint.entity0 = parent;
            joint.entity1 = selection->selected[1];

            auto createJoint = [&](JointType type, string_view name) {
               joint.type = type;

               auto entity = CreateEmpty(scene, name, parent, vec3_Zero);
               entity.Add<JointComponent>(std::move(joint));
               return entity;
            };

            if (ImGui::MenuItem("Fixed")) {
               return createJoint(JointType::Fixed, "Fixed Joint");
            }

            if (ImGui::MenuItem("Distance")) {
               return createJoint(JointType::Distance, "Distance Joint");
            }

            if (ImGui::MenuItem("Revolute")) {
               return createJoint(JointType::Revolute, "Revolute Joint");
            }

            if (ImGui::MenuItem("Spherical")) {
               return createJoint(JointType::Spherical, "Spherical Joint");
            }

            if (ImGui::MenuItem("Prismatic")) {
               return createJoint(JointType::Prismatic, "Prismatic Joint");
            }
         }
      }

      if (UI_MENU("Lighting")) {
         if (ImGui::MenuItem("Create Direct Light")) {
            return CreateDirectLight(scene);
         }
         if (ImGui::MenuItem("Create Sky")) {
            return CreateSky(scene);
         }
      }

      if (UI_MENU("Presets")) {
         vec3 cubeSize{ 25, 10, 25 };

         if (ImGui::MenuItem("Random Cubes")) {
            Entity root = scene.Create("Random Cubes");

            for (int i = 0; i < 500; ++i) {
               CreateCube(scene, CubeDesc{
                  .parent = root,
                  .pos = Random::Float3(-cubeSize, cubeSize),
                  .scale = Random::Float3(vec3{ 0 }, vec3{ 3.f }),
                  .rotation = Random::Float3(vec3{ 0 }, vec3{ 30.f }), // todo: PI?
                  .color = Random::Color() });
            }

            return root;
         }

         if (ImGui::MenuItem("Random Lights")) {
            Entity root = scene.Create("Random Lights");

            for (int i = 0; i < 8; ++i) {
               Entity e = CreateEmpty(scene, std::format("Light {}", i),
                  root, Random::Float3(-cubeSize, cubeSize));

               auto& light = e.Add<LightComponent>();
               light.color = Random::Float3(vec3{ 0 }, vec3{ 1.f });
               light.intensity = Random::Float(0.f, 20.f);
               light.radius = Random::Float(3, 10);
            }

            return root;
         }

         if (ImGui::MenuItem("Add Geom if Material is presents")) {
            auto view = scene.View<MaterialComponent>();
            for (auto _e : view) {
               Entity e{ _e, &scene };
               auto& geom = e.GetOrAdd<GeometryComponent>();
               geom.type = GeomType::Box;
            }
         }

         if (ImGui::MenuItem("Create wall")) {
            Entity root = scene.Create("Wall");
            root.GetTransform().position = spawnPosHint;

            int size = 10;
            for (int y = 0; y < size; ++y) {
               for (int x = 0; x < size; ++x) {
                  CreateCube(scene, CubeDesc{
                     .parent = root,
                     .pos = vec3{ -size / 2.f + x, y + 0.5f, 0 },
                     .color = Random::Color() });
               }
            }

            return root;
         }

         if (ImGui::MenuItem("Create stack tri")) {
            Entity root = scene.Create("Stack tri");
            root.GetTransform().position = spawnPosHint;

            int size = 10;
            for (int y = 0; y < size; ++y) {
               int width = size - y;
               for (int x = 0; x < width; ++x) {
                  CreateCube(scene, CubeDesc{
                     .parent = root,
                     .pos = vec3{ -width / 2.f + x, y + 0.5f, 0 },
                     .color = Random::Color() });
               }
            }

            return root;
         }

         if (ImGui::MenuItem("Create stack")) {
            Entity root = scene.Create("Stack");
            root.GetTransform().position = spawnPosHint;

            int size = 10;
            for (int i = 0; i < size; ++i) {
               CreateCube(scene, CubeDesc{
                  .parent = root,
                  .pos = vec3{0, i + 0.5f, 0},
                  .color = Random::Color() });
            }

            return root;
         }

         if (ImGui::MenuItem("Create chain")) {
            Entity root = CreateCube(scene, CubeDesc{
               .namePrefix = "Chain",
               .pos = spawnPosHint,
               .dynamic = false,
               .color = Random::Color() });

            Entity prev = root;
            int size = 10;
            for (int i = 0; i < size - 1; ++i) {
               Entity cur = CreateCube(scene, CubeDesc{
                  .parent = root,
                  .pos = vec3{0, i * 2 + 0.5f, 0},
                  .color = Random::Color() });

               JointComponent joint{};
               joint.entity0 = prev;
               joint.entity1 = cur;
               joint.type = JointType::Distance;
               joint.minDistance = 1;
               joint.minDistance = 2;

               auto jointEntity = CreateEmpty(scene, "Distance Joint", root, vec3_Zero);
               jointEntity.Add<JointComponent>(std::move(joint));

               prev = cur;
            }

            return root;
         }
      }

      if (ImGui::MenuItem("Create Decal")) {
         auto createdEntity = scene.Create();
         createdEntity.Add<DecalComponent>();
         return createdEntity;
      }

      return {};
   }

}
