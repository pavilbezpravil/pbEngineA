#include "pch.h"
#include "Component.h"

#include "typer/Typer.h"
#include "scene/Entity.h"

#include <glm/gtx/matrix_decompose.hpp>

#include "gui/Gui.h"

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
      TYPER_FIELD(baseColor)

      TYPE_FIELD_UI(UISliderFloat{ .min = 0, .max = 1 })
      TYPER_FIELD(roughness)

      TYPE_FIELD_UI(UISliderFloat{ .min = 0, .max = 1 })
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
      TYPE_FIELD_UI(UIColorEdit3)
      TYPE_FIELD(color)

      TYPE_FIELD_UI(UISliderFloat{ .min = 0, .max = 10 })
      TYPE_FIELD(intensity)
   TYPER_END(DirectLightComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(DirectLightComponent)

   TYPER_BEGIN(DecalComponent)
      TYPER_FIELD(baseColor)
      TYPER_FIELD(metallic)
      TYPER_FIELD(roughness)
   TYPER_END(DecalComponent)

   COMPONENT_EXPLICIT_TEMPLATE_DEF(DecalComponent)

   TYPER_BEGIN(SkyComponent)
      TYPE_FIELD(directLight)

      TYPE_FIELD_UI(UIColorEdit3)
      TYPE_FIELD(color)
   TYPER_END(SkyComponent)

   TYPER_BEGIN(WaterComponent)
      TYPE_FIELD_UI(UIColorEdit3)
      TYPE_FIELD(fogColor)

      TYPE_FIELD(fogUnderwaterLength)
      TYPE_FIELD(softZ)
   TYPER_END(WaterComponent)

   TYPER_BEGIN(TerrainComponent)
      TYPE_FIELD_UI(UIColorEdit3)
      TYPE_FIELD(color)
   TYPER_END(TerrainComponent)

   ComponentRegisterGuard::~ComponentRegisterGuard() {
      Typer::Get().UnregisterComponent(typeID);
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

   void RegisterBasicComponents(Typer& typer) {
      ComponentInfo ci{};

      INTERNAL_ADD_COMPONENT(SceneTransformComponent);
      INTERNAL_ADD_COMPONENT(SimpleMaterialComponent);
      INTERNAL_ADD_COMPONENT(LightComponent);
      INTERNAL_ADD_COMPONENT(DirectLightComponent);
      INTERNAL_ADD_COMPONENT(DecalComponent);
      INTERNAL_ADD_COMPONENT(SkyComponent);
      INTERNAL_ADD_COMPONENT(WaterComponent);
      INTERNAL_ADD_COMPONENT(TerrainComponent);
   }

}
