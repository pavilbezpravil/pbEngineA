#include "pch.h"
#include "Utils.h"

#include "Component.h"
#include "../../../pbeEditor/src/EditorSelection.h" // todo:
#include "app/Input.h"
#include "gui/Gui.h"
#include "math/Color.h"
#include "math/Common.h"
#include "math/Random.h"
#include "physics/PhysComponents.h"
#include "physics/PhysicsScene.h"

namespace pbe {

   // todo:
   static vec3 MiddlePointForEntities(std::span<Entity> entities) {
      vec3 midPoint = vec3_Zero;
      for (auto entity : entities) {
         midPoint += entity.GetTransform().Position();
      }
      midPoint /= entities.empty() ? 1.f : (float)entities.size();

      return midPoint;
   }

   static Entity CreateParentForEntities(Scene& scene, std::span<Entity> entities) {
      vec3 midPoint = MiddlePointForEntities(entities);

      Entity parentForSelected = CreateEmpty(scene, "Parent for selected", {}, midPoint);
      for (auto entity : entities) {
         entity.GetTransform().SetParent(parentForSelected);
      }

      return parentForSelected;
   }

   static Entity CreateEmpty(Scene& scene, string_view namePrefix, Entity parent, const vec3& pos, Space space) {
      // todo: find appropriate name
      auto entity = scene.Create(parent, namePrefix);
      entity.Get<SceneTransformComponent>().SetPosition(pos, space);
      return entity;
   }

   // todo: move to scene transform component
   template<typename F>
   static void SceneHierApplyFunc(Entity entity, F&& func) {
      func(entity);

      auto& trans = entity.GetTransform();
      for (auto& childEntity : trans.children) {
         SceneHierApplyFunc(childEntity, func);
      }
   }

   Entity CreateCube(Scene& scene, const CubeDesc& desc) {
      auto entity = CreateEmpty(scene, desc.namePrefix, desc.parent, desc.pos, desc.space);

      auto& trans = entity.Get<SceneTransformComponent>();
      trans.SetScale(desc.scale, desc.space);
      trans.SetRotation(desc.rotation, desc.space);

      trans.UpdatePrevTransform();

      entity.Add<GeometryComponent>();

      if (desc.type & CubeDesc::Material) {
         entity.Add<MaterialComponent>().baseColor = desc.color;
      }

      if (desc.type & CubeDesc::RigidBodyShape) {
         entity.Add<RigidBodyShapeComponent>();
      }

      if (desc.type & (CubeDesc::RigidBodyStatic | CubeDesc::RigidBodyDynamic)) {
         // todo:
         RigidBodyComponent _rb{};
         _rb.dynamic = (desc.type & CubeDesc::RigidBodyDynamic) != 0;
         entity.Add<RigidBodyComponent>(_rb);
      }

      return entity;
   }

   Entity CreateLight(Scene& scene, string_view namePrefix, const vec3& pos) {
      auto entity = CreateEmpty(scene, namePrefix, {}, pos);
      entity.Add<LightComponent>();
      return entity;
   }

   Entity CreateDirectLight(Scene& scene, string_view namePrefix, const vec3& pos) {
      auto entity = CreateEmpty(scene, namePrefix, {}, pos);
      entity.Get<SceneTransformComponent>().Local().rotation = quat{ vec3{PIHalf * 0.5, PIHalf * 0.5, 0 } };
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
      Entity parent = selection ? selection->LastSelected() : Entity{};

      if (ImGui::MenuItem("Create Empty")) {
         return CreateEmpty(scene, "Empty", parent);
      }

      CubeDesc cubeDesc{ .parent = parent, .pos = spawnPosHint };

      if (UI_MENU("Geometry")) {
         if (ImGui::MenuItem("Cube")) {
            cubeDesc.type = CubeDesc::GeomMaterial;
            return CreateCube(scene, cubeDesc);
         }
      }

      if (UI_MENU("Physics")) {
         if (ImGui::MenuItem("Dynamic Cube")) {
            cubeDesc.type = CubeDesc::PhysDynamic;
            return CreateCube(scene, cubeDesc);
         }
         if (ImGui::MenuItem("Static Cube")) {
            cubeDesc.type = CubeDesc::PhysStatic;
            return CreateCube(scene, cubeDesc);
         }
         if (ImGui::MenuItem("Shape")) {
            cubeDesc.type = CubeDesc::PhysShape;
            return CreateCube(scene, cubeDesc);
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
         if (ImGui::MenuItem("Light")) {
            return CreateLight(scene);
         }
         if (ImGui::MenuItem("Create Direct Light")) {
            return CreateDirectLight(scene);
         }
         if (ImGui::MenuItem("Create Sky")) {
            return CreateSky(scene);
         }
      }

      // if (Input::IsKeyDown(KeyCode::A)) {
      //    ImGui::OpenPopup("Actions");
      // }
      if (UI_MENU("Actions")) {
         uint nSelected = selection ? (uint)selection->selected.size() : 0;

         if (ImGui::MenuItem("Create parent for selected", 0, false, nSelected >= 2)) {
            return CreateParentForEntities(scene, selection->selected);
         }
         if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create new entity at middle point and set one as parent for selected entities");
         }

         if (ImGui::MenuItem("Last selected as parent for rest", 0, false, nSelected >= 2)) {
            Entity parentForSelected = selection->LastSelected();

            for (auto entity : selection->selected | std::views::reverse | std::views::drop(1)) {
               entity.GetTransform().SetParent(parentForSelected);
            }
            return parentForSelected;
         }
         if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Make last selected entity as parent for rest selected entities");
         }

         ImGui::Separator();

         if (ImGui::MenuItem("Add static rb", 0, false, nSelected >= 1)) {
            for (auto entity : selection->selected) {
               RigidBodyComponent& rb = entity.GetOrAdd<RigidBodyComponent>();
               rb.dynamic = false;
               entity.MarkComponentUpdated<RigidBodyComponent>();
            }
            // todo: notify
         }
         if (ImGui::MenuItem("Add dynamic rb", 0, false, nSelected >= 1)) {
            for (auto entity : selection->selected) {
               RigidBodyComponent& rb = entity.GetOrAdd<RigidBodyComponent>();
               rb.dynamic = true;
               entity.MarkComponentUpdated<RigidBodyComponent>();
            }
         }
         if (ImGui::MenuItem("Make destructible", 0, false, nSelected >= 1)) {
            for (auto entity : selection->selected) {
               RigidBodyComponent& rb = entity.GetOrAdd<RigidBodyComponent>();
               rb.destructible = true;
               entity.MarkComponentUpdated<RigidBodyComponent>();
            }
         }

         ImGui::Separator();

         if (ImGui::MenuItem("Copy material from last selected", 0, false, nSelected >= 2)) {
            MaterialComponent* materialForCopy = selection->LastSelected().TryGet<MaterialComponent>();

            if (materialForCopy) {
               for (auto entity : selection->selected | std::views::drop(1)) {
                  if (auto material = entity.TryGet<MaterialComponent>()) {
                     *material = *materialForCopy;
                  }
               }
            }
         }

         ImGui::Separator();

         // todo: random scale
         float rndScale = 0.3f;

         // todo: iterate on children too
         if (ImGui::MenuItem("Randomize base color", 0, false, nSelected >= 1)) {
            for (auto entity : selection->selected) {
               // if (auto material = entity.TryGet<MaterialComponent>()) {
               //    material->baseColor = Saturate(material->baseColor + Random::Float3(vec3{-1}, vec3{1}) * rndScale);
               // }

               SceneHierApplyFunc(entity,
                  [rndScale] (Entity& entity)
                  {
                     if (auto material = entity.TryGet<MaterialComponent>()) {
                        material->baseColor = Saturate(material->baseColor + Random::Float3(vec3{ -1 }, vec3{ 1 }) * rndScale);
                     }
                  });
            }
         }

         if (ImGui::MenuItem("Randomize roughness", 0, false, nSelected >= 1)) {
            for (auto entity : selection->selected) {
               if (auto material = entity.TryGet<MaterialComponent>()) {
                  material->roughness = Saturate(material->roughness + Random::Float(-1, 1) * rndScale);
               }
            }
         }

         if (ImGui::MenuItem("Randomize metalness", 0, false, nSelected >= 1)) {
            for (auto entity : selection->selected) {
               if (auto material = entity.TryGet<MaterialComponent>()) {
                  material->metallic = Saturate(material->metallic + Random::Float(-1, 1) * rndScale);
               }
            }
         }
      }

      if (UI_MENU("Presets")) {
         vec3 cubeSize{ 25, 10, 25 };

         if (ImGui::MenuItem("Random Cubes")) {
            Entity root = scene.Create("Random Cubes");

            for (int i = 0; i < 500; ++i) {
               CreateCube(scene, CubeDesc{
                  .parent = root,
                  .space = Space::Local,
                  .pos = Random::Float3(-cubeSize, cubeSize),
                  .rotation = Random::Float3(vec3{ 0 }, vec3{ 30.f }), // todo: PI?
                  .scale = Random::Float3(vec3{ 0 }, vec3{ 3.f }),
                  .color = Random::Color() });
            }

            return root;
         }

         if (ImGui::MenuItem("Random Lights")) {
            Entity root = scene.Create("Random Lights");

            for (int i = 0; i < 8; ++i) {
               Entity e = CreateEmpty(scene, std::format("Light {}", i),
                  root, Random::Float3(-cubeSize, cubeSize), Space::Local);

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
            root.GetTransform().Local().position = spawnPosHint;

            int size = 10;
            for (int y = 0; y < size; ++y) {
               for (int x = 0; x < size; ++x) {
                  CreateCube(scene, CubeDesc{
                     .parent = root,
                     .space = Space::Local,
                     .pos = vec3{ -size / 2.f + x, y + 0.5f, 0 },
                     .color = Random::Color() });
               }
            }

            return root;
         }

         if (ImGui::MenuItem("Create stack tri")) {
            Entity root = scene.Create("Stack tri");
            root.GetTransform().Local().position = spawnPosHint;

            int size = 10;
            for (int y = 0; y < size; ++y) {
               int width = size - y;
               for (int x = 0; x < width; ++x) {
                  CreateCube(scene, CubeDesc{
                     .parent = root,
                     .space = Space::Local,
                     .pos = vec3{ -width / 2.f + x, y + 0.5f, 0 },
                     .color = Random::Color() });
               }
            }

            return root;
         }

         if (ImGui::MenuItem("Create stack")) {
            Entity root = scene.Create("Stack");
            root.GetTransform().Local().position = spawnPosHint;

            int size = 10;
            for (int i = 0; i < size; ++i) {
               CreateCube(scene, CubeDesc{
                  .parent = root,
                  .space = Space::Local,
                  .pos = vec3{0, i + 0.5f, 0},
                  .color = Random::Color() });
            }

            return root;
         }

         if (ImGui::MenuItem("Create chain")) {
            Entity root = CreateCube(scene, CubeDesc{
               .namePrefix = "Chain",
               .pos = spawnPosHint,
               .color = Random::Color(),
               .type = CubeDesc::PhysStatic,
            });

            Entity prev = root;
            int size = 10;
            for (int i = 0; i < size - 1; ++i) {
               Entity cur = CreateCube(scene, CubeDesc{
                  .parent = root,
                  .space = Space::Local,
                  .pos = vec3{0, i * 2 + 0.5f, 0},
                  .color = Random::Color() });

               JointComponent joint{};
               joint.entity0 = prev;
               joint.entity1 = cur;
               joint.type = JointType::Distance;
               joint.distance.minDistance = 1;
               joint.distance.maxDistance = 2;
               joint.distance.stiffness = 0;

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
