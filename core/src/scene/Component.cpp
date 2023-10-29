#include "pch.h"
#include "Component.h"

#include "typer/Typer.h"
#include "scene/Entity.h"

#include "gui/Gui.h"
#include "math/Random.h"
#include "typer/Registration.h"
#include "typer/Serialize.h"
#include "physics/PhysXTypeConvet.h"

namespace pbe {

   bool GeomUI(const char* name, byte* value) {
      auto& geom = *(GeometryComponent*)value;

      bool editted = false;

      // editted |= ImGui::Combo("Type", (int*)&geom.type, "Sphere\0Box\0Cylinder\0Cone\0Capsule\0");
      editted |= EditorUI("type", geom.type);

      if (geom.type == GeomType::Sphere) {
         editted |= ImGui::InputFloat("Radius", &geom.sizeData.x);
      } else if (geom.type == GeomType::Box) {
         editted |= ImGui::InputFloat3("Size", &geom.sizeData.x);
      } else {
         // Cylinder, Cone, Capsule
         editted |= ImGui::InputFloat("Radius", &geom.sizeData.x);
         editted |= ImGui::InputFloat("Height", &geom.sizeData.y);
      }

      return editted;
   }

   STRUCT_BEGIN(TagComponent)
      STRUCT_FIELD(tag)
   STRUCT_END()

   STRUCT_BEGIN(CameraComponent)
      STRUCT_FIELD(main)
   STRUCT_END()

   STRUCT_BEGIN(MaterialComponent)
      STRUCT_FIELD_UI(UIColorEdit3)
      STRUCT_FIELD(baseColor)

      STRUCT_FIELD_UI(UISliderFloat{ .min = 0, .max = 1 })
      STRUCT_FIELD(roughness)

      STRUCT_FIELD_UI(UISliderFloat{ .min = 0, .max = 1 })
      STRUCT_FIELD(metallic)

      STRUCT_FIELD(emissivePower)

      STRUCT_FIELD(opaque)
   STRUCT_END()

   ENUM_BEGIN(GeomType)
      ENUM_VALUE(Sphere)
      ENUM_VALUE(Box)
      ENUM_VALUE(Cylinder)
      ENUM_VALUE(Cone)
      ENUM_VALUE(Capsule)
   ENUM_END()

   STRUCT_BEGIN(GeometryComponent)
      TYPE_UI(GeomUI)
      STRUCT_FIELD(type)
      STRUCT_FIELD(sizeData)
   STRUCT_END()

   STRUCT_BEGIN(LightComponent)
      STRUCT_FIELD_UI(UIColorEdit3)
      STRUCT_FIELD(color)
      STRUCT_FIELD(intensity)
      STRUCT_FIELD(radius)
   STRUCT_END()

   STRUCT_BEGIN(DirectLightComponent)
      STRUCT_FIELD_UI(UIColorEdit3)
      STRUCT_FIELD(color)

      STRUCT_FIELD_UI(UISliderFloat{ .min = 0, .max = 10 })
      STRUCT_FIELD(intensity)
   STRUCT_END()

   STRUCT_BEGIN(RTImportanceVolumeComponent)
      STRUCT_FIELD(radius)
   STRUCT_END()

   STRUCT_BEGIN(DecalComponent)
      STRUCT_FIELD(baseColor)
      STRUCT_FIELD(metallic)
      STRUCT_FIELD(roughness)
   STRUCT_END()

   STRUCT_BEGIN(SkyComponent)
      STRUCT_FIELD(directLight)

      STRUCT_FIELD_UI(UIColorEdit3)
      STRUCT_FIELD(color)
      STRUCT_FIELD(intensity)
   STRUCT_END()

   STRUCT_BEGIN(WaterComponent)
      STRUCT_FIELD_UI(UIColorEdit3)
      STRUCT_FIELD(fogColor)

      STRUCT_FIELD(fogUnderwaterLength)
      STRUCT_FIELD(softZ)
   STRUCT_END()

   STRUCT_BEGIN(TerrainComponent)
      STRUCT_FIELD_UI(UIColorEdit3)
      STRUCT_FIELD(color)
   STRUCT_END()

   void TimedDieComponent::SetRandomDieTime(float min, float max) {
      time = Random::Float(min, max);
   }

   void RegisterBasicComponents(Typer& typer) {
      // INTERNAL_ADD_COMPONENT(SceneTransformComponent);
      INTERNAL_ADD_COMPONENT(CameraComponent);
      INTERNAL_ADD_COMPONENT(MaterialComponent);
      INTERNAL_ADD_COMPONENT(GeometryComponent);
      INTERNAL_ADD_COMPONENT(LightComponent);
      INTERNAL_ADD_COMPONENT(DirectLightComponent);
      INTERNAL_ADD_COMPONENT(RTImportanceVolumeComponent);
      INTERNAL_ADD_COMPONENT(DecalComponent);
      INTERNAL_ADD_COMPONENT(SkyComponent);
      INTERNAL_ADD_COMPONENT(WaterComponent);
      INTERNAL_ADD_COMPONENT(TerrainComponent);
   }

}
