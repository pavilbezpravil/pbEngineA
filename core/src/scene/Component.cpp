#include "pch.h"
#include "Component.h"

#include "gui/Gui.h"
#include "typer/Typer.h"
#include "scene/Entity.h"

#include <glm/gtx/matrix_decompose.hpp>

namespace pbe {

   COMPONENT_EXPLICIT_TEMPLATE_DEF(UUIDComponent);

   TYPER_BEGIN(TagComponent)
      TYPER_FIELD(tag)
   TYPER_END(TagComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(TagComponent);

   TYPER_BEGIN(SceneTransformComponent)
      TYPER_FIELD(position)
      TYPER_FIELD(rotation)
      TYPER_FIELD(scale)
   TYPER_END(SceneTransformComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(SceneTransformComponent)

   TYPER_BEGIN(SimpleMaterialComponent)
      TYPER_FIELD(albedo)
      TYPER_FIELD(roughness)
      TYPER_FIELD(metallic)
      TYPER_FIELD(opaque)
   TYPER_END(SimpleMaterialComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(SimpleMaterialComponent)

   TYPER_BEGIN(LightComponent)
      TYPER_FIELD(color)
      TYPER_FIELD(intensity)
      TYPER_FIELD(radius)
   TYPER_END(LightComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(LightComponent)

   TYPER_BEGIN(DirectLightComponent)
      TYPER_FIELD(color)
      TYPER_FIELD(intensity)
   TYPER_END(DirectLightComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(DirectLightComponent)

   TYPER_BEGIN(DecalComponent)
      TYPER_FIELD(albedo)
      TYPER_FIELD(metallic)
      TYPER_FIELD(roughness)
   TYPER_END(DecalComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(DecalComponent)

   ComponentList& ComponentList::Get() {
      static ComponentList cl;
      return cl;
   }

   void ComponentList::RegisterComponent(ComponentID componentID, ComponentFunc&& func) {
      components2[componentID] = std::move(func);
   }

   vec3 SceneTransformComponent::Up() const {
      return rotation * vec3_Up;
   }

   vec3 SceneTransformComponent::Forward() const {
      return rotation * vec3_Forward;
   }

   mat4 SceneTransformComponent::GetMatrix() const {
      mat4 transform = glm::translate(mat4(1), position);
      transform *= mat4{ rotation };
      transform *= glm::scale(mat4(1), scale);
      return transform;
   }

   std::tuple<glm::vec3, glm::quat, glm::vec3> GetTransformDecomposition(const glm::mat4& transform) {
      glm::vec3 scale, translation, skew;
      glm::vec4 perspective;
      glm::quat orientation;
      glm::decompose(transform, scale, orientation, translation, skew, perspective);

      return { translation, orientation, scale };
   }

   void SceneTransformComponent::SetMatrix(const mat4& transform) {
      auto [position_, rotation_, scale_] = GetTransformDecomposition(transform);

      position = position_;
      rotation = rotation_;
      scale = scale_;
   }

   int RegisterComponents() {
      TypeID id;

      id = GetTypeID<SceneTransformComponent>();
      ComponentList::Get().RegisterComponent(id, [](Entity& entity) {
         if (auto* c = entity.TryGet<SceneTransformComponent>()) {
            EditorUI<SceneTransformComponent>(c->GetName(), *c);
         }
         });

      id = GetTypeID<SimpleMaterialComponent>();
      ComponentList::Get().RegisterComponent(id, [](Entity& entity) {
         if (auto* c = entity.TryGet<SimpleMaterialComponent>()) {
            EditorUI<SimpleMaterialComponent>(c->GetName(), *c);
         }
         });

      id = GetTypeID<LightComponent>();
      ComponentList::Get().RegisterComponent(id, [](Entity& entity) {
         if (auto* c = entity.TryGet<LightComponent>()) {
            EditorUI<LightComponent>(c->GetName(), *c);
         }
         });

      id = GetTypeID<DecalComponent>();
      ComponentList::Get().RegisterComponent(id, [](Entity& entity) {
         if (auto* c = entity.TryGet<DecalComponent>()) {
            EditorUI<DecalComponent>(c->GetName(), *c);
         }
         });

      return 0;
   }

   static int __unused_value = RegisterComponents();

}
