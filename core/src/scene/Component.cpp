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

   TYPER_BEGIN(TagComponent)
      TYPER_FIELD(tag)
   TYPER_END(TagComponent)

   TYPER_BEGIN(SceneTransformComponent)
      TYPER_SERIALIZE(TransSerialize)
      TYPER_DESERIALIZE(TransDeserialize)

      TYPER_FIELD(position)
      TYPER_FIELD(rotation)
      TYPER_FIELD(scale)
   TYPER_END(SceneTransformComponent)

   TYPER_BEGIN(SimpleMaterialComponent)
      TYPE_FIELD_UI(UIColorEdit3)
      TYPER_FIELD(baseColor)

      TYPE_FIELD_UI(UISliderFloat{ .min = 0, .max = 1 })
      TYPER_FIELD(roughness)

      TYPE_FIELD_UI(UISliderFloat{ .min = 0, .max = 1 })
      TYPER_FIELD(metallic)

      TYPER_FIELD(opaque)
   TYPER_END(SimpleMaterialComponent)

   TYPER_BEGIN(LightComponent)
      TYPER_FIELD(color)
      TYPER_FIELD(intensity)
      TYPER_FIELD(radius)
   TYPER_END(LightComponent)

   TYPER_BEGIN(DirectLightComponent)
      TYPE_FIELD_UI(UIColorEdit3)
      TYPE_FIELD(color)

      TYPE_FIELD_UI(UISliderFloat{ .min = 0, .max = 10 })
      TYPE_FIELD(intensity)
   TYPER_END(DirectLightComponent)

   TYPER_BEGIN(DecalComponent)
      TYPER_FIELD(baseColor)
      TYPER_FIELD(metallic)
      TYPER_FIELD(roughness)
   TYPER_END(DecalComponent)

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

   void SceneTransformComponent::SetParent(Entity newParent) {
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
