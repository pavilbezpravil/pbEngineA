#include "pch.h"
#include "Component.h"

#include "typer/Typer.h"
#include "scene/Entity.h"

#include <glm/gtx/matrix_decompose.hpp>

#include "gui/Gui.h"
#include "typer/Serialize.h"

namespace pbe {

   void TransSerialize(Serializer& ser, const byte* value) {
      auto& trans = *(const SceneTransformComponent*)value;

      SERIALIZER_MAP(ser);

      ser.Ser("position", trans.position);
      ser.Ser("rotation", trans.rotation);
      ser.Ser("scale", trans.scale);

      if (trans.HasParent()) {
         ser.KeyValue("parent", trans.parent.Get<UUIDComponent>().uuid);
      }

      if (trans.HasChilds()) {
         ser.Key("children");

         auto& out = ser.out;
         out << YAML::Flow;
         SERIALIZER_SEQ(ser);

         for (auto children : trans.children) {
            out << (uint64)children.Get<UUIDComponent>().uuid;
         }
      }
   };

   void TransDeserialize(const Deserializer& deser, byte* value) {
      auto& trans = *(SceneTransformComponent*)value;

      // todo:
      trans.RemoveAllChild();

      deser.Deser("position", trans.position);
      deser.Deser("rotation", trans.rotation);
      deser.Deser("scale", trans.scale);

      // note: we will be added by our parent
      // auto parent = deser.Deser<uint64>("parent");

      if (auto children = deser["children"]) {
         trans.children.reserve(children.node.size());

         for (auto child : children.node) {
            uint64 childUuid = child.as<uint64>();
            trans.AddChild(trans.entity.GetScene()->GetEntity(childUuid));
         }
      }
   }

   bool GeomUI(const char* name, byte* value) {
      auto& geom = *(GeometryComponent*)value;

      bool editted = false;

      if (UI_TREE_NODE(name, ImGuiTreeNodeFlags_SpanFullWidth)) {
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
      }

      return editted;
   }

   TYPER_BEGIN(TagComponent)
      TYPER_FIELD(tag)
   TYPER_END()

   TYPER_BEGIN(SceneTransformComponent)
      TYPER_SERIALIZE(TransSerialize)
      TYPER_DESERIALIZE(TransDeserialize)

      TYPER_FIELD(position)
      TYPER_FIELD(rotation)
      TYPER_FIELD(scale)
   TYPER_END()

   TYPER_BEGIN(SimpleMaterialComponent)
      TYPER_FIELD_UI(UIColorEdit3)
      TYPER_FIELD(baseColor)

      TYPER_FIELD_UI(UISliderFloat{ .min = 0, .max = 1 })
      TYPER_FIELD(roughness)

      TYPER_FIELD_UI(UISliderFloat{ .min = 0, .max = 1 })
      TYPER_FIELD(metallic)

      TYPER_FIELD_UI(UIColorEdit3)
      TYPER_FIELD(emissiveColor)

      TYPER_FIELD(emissivePower)

      TYPER_FIELD(opaque)
   TYPER_END()

   TYPER_BEGIN(GeomType)
      TYPER_SERIALIZE([](Serializer& ser, const byte* value) { ser.out << *(int*)value; })
      TYPER_DESERIALIZE([](const Deserializer& deser, byte* value) { *(int*)value = deser.node.as<int>(); })
      TYPER_UI([](const char* name, byte* value) { return ImGui::Combo(name, (int*)value, "Sphere\0Box\0Cylinder\0Cone\0Capsule\0"); })
   TYPER_END()

   // TYPER_BEGIN(GeomType)
   // todo: try
      // TYPER_FIELD_UI(UICombo{ .items = { "Box", "Sphere", "Cylinder", "Capsule", "Cone", "Plane" } })
      // TYPER_FIELD(type)
   // TYPER_END(GeomType)

   TYPER_BEGIN(GeometryComponent)
      TYPER_UI(GeomUI)
      TYPER_FIELD(type)
      TYPER_FIELD(sizeData)
   TYPER_END()

   TYPER_BEGIN(LightComponent)
      TYPER_FIELD(color)
      TYPER_FIELD(intensity)
      TYPER_FIELD(radius)
   TYPER_END()

   TYPER_BEGIN(DirectLightComponent)
      TYPER_FIELD_UI(UIColorEdit3)
      TYPER_FIELD(color)

      TYPER_FIELD_UI(UISliderFloat{ .min = 0, .max = 10 })
      TYPER_FIELD(intensity)
   TYPER_END()

   TYPER_BEGIN(DecalComponent)
      TYPER_FIELD(baseColor)
      TYPER_FIELD(metallic)
      TYPER_FIELD(roughness)
   TYPER_END()

   TYPER_BEGIN(SkyComponent)
      TYPER_FIELD(directLight)

      TYPER_FIELD_UI(UIColorEdit3)
      TYPER_FIELD(color)
      TYPER_FIELD(intensity)
   TYPER_END()

   TYPER_BEGIN(WaterComponent)
      TYPER_FIELD_UI(UIColorEdit3)
      TYPER_FIELD(fogColor)

      TYPER_FIELD(fogUnderwaterLength)
      TYPER_FIELD(softZ)
   TYPER_END()

   TYPER_BEGIN(TerrainComponent)
      TYPER_FIELD_UI(UIColorEdit3)
      TYPER_FIELD(color)
   TYPER_END()

   void __ComponentUnreg(TypeID typeID) {
      Typer::Get().UnregisterComponent(typeID);
   }

   vec3 SceneTransformComponent::Position() const {
      vec3 pos = position;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         pos = pTrans.Position() + pTrans.Rotation() * (pos * pTrans.Scale());
      }
      return pos;
   }

   quat SceneTransformComponent::Rotation() const {
      quat rot = rotation;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         rot = pTrans.Rotation() * rot;
      }
      return rot;
   }

   vec3 SceneTransformComponent::Scale() const {
      vec3 s = scale;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         s *= pTrans.Scale();
      }
      return s;
   }

   void SceneTransformComponent::SetPosition(const vec3& pos) {
      if (HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         position = glm::inverse(pTrans.Rotation()) * (pos - pTrans.Position()) / pTrans.Scale();
      } else {
         position = pos;
      }
   }

   void SceneTransformComponent::SetRotation(const quat& rot) {
      if (HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         rotation = glm::inverse(pTrans.Rotation()) * rot;
      } else {
         rotation = rot;
      }
   }

   void SceneTransformComponent::SetScale(const vec3& s) {
      if (HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         scale = s / pTrans.Scale();
      } else {
         scale = s;
      }
   }

   vec3 SceneTransformComponent::Right() const {
      return Rotation() * vec3_Right;
   }

   vec3 SceneTransformComponent::Up() const {
      return Rotation() * vec3_Up;
   }

   vec3 SceneTransformComponent::Forward() const {
      return Rotation() * vec3_Forward;
   }

   mat4 SceneTransformComponent::GetMatrix() const {
      mat4 transform = glm::translate(mat4(1), Position());
      transform *= mat4{ Rotation() };
      transform *= glm::scale(mat4(1), Scale());
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

      // set in world space
      SetPosition(position_);
      SetRotation(rotation_);
      SetScale(scale_);
   }

   void SceneTransformComponent::AddChild(Entity child) {
      child.Get<SceneTransformComponent>().SetParent(entity);
   }

   void SceneTransformComponent::RemoveChild(int idx) {
      ASSERT(idx >= 0 && idx < children.size());
      children[idx].Get<SceneTransformComponent>().SetParent(); // todo: mb set to scene root?
   }

   void SceneTransformComponent::RemoveAllChild(Entity theirNewParent) {
      for (int i = (int)children.size() - 1; i >= 0; --i) {
         children[i].Get<SceneTransformComponent>().SetParent(theirNewParent);
      }
      ASSERT(!HasChilds());
   }

   bool SceneTransformComponent::SetParent(Entity newParent) {
      if (newParent == entity) {
         return false;
      }

      auto pos = Position();
      auto rot = Rotation();
      auto scale = Scale();

      if (HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         std::erase(pTrans.children, entity);
      }

      parent = newParent;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         pTrans.children.push_back(entity); // todo: set by idx?
      }

      SetPosition(pos);
      SetRotation(rot);
      SetScale(scale);

      return true;
   }

   void RegisterBasicComponents(Typer& typer) {
      INTERNAL_ADD_COMPONENT(SceneTransformComponent);
      INTERNAL_ADD_COMPONENT(SimpleMaterialComponent);
      INTERNAL_ADD_COMPONENT(GeometryComponent);
      INTERNAL_ADD_COMPONENT(LightComponent);
      INTERNAL_ADD_COMPONENT(DirectLightComponent);
      INTERNAL_ADD_COMPONENT(DecalComponent);
      INTERNAL_ADD_COMPONENT(SkyComponent);
      INTERNAL_ADD_COMPONENT(WaterComponent);
      INTERNAL_ADD_COMPONENT(TerrainComponent);
   }

}
