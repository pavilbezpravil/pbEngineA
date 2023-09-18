#include "pch.h"
#include "Component.h"

#include "typer/Typer.h"
#include "scene/Entity.h"

#include <glm/gtx/matrix_decompose.hpp>

#include "imgui_internal.h"
#include "gui/Gui.h"
#include "typer/Registration.h"
#include "typer/Serialize.h"
#include "physics/PhysXTypeConvet.h"

namespace pbe {

   bool Vec3UI(const char* label, vec3& v, float resetVal, float columnWidth) {
      UI_PUSH_ID(label);

      ImGui::Columns(2);

      ImGui::SetColumnWidth(-1, columnWidth);
      ImGui::Text(label);
      ImGui::NextColumn();

      ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

      bool edited = false;

      auto drawFloat = [&] (float& val, const ImVec4& color, const char* button) {
         {
            UI_PUSH_STYLE_COLOR(ImGuiCol_Button, color);
            UI_PUSH_STYLE_COLOR(ImGuiCol_ButtonHovered, (color + ImVec4{0.1f, 0.1f, 0.1f, 1}));
            UI_PUSH_STYLE_COLOR(ImGuiCol_ButtonActive, color);
            if (ImGui::Button(button)) {
               val = resetVal;
               edited = true;
            }
         }

         ImGui::SameLine();

         {
            UI_PUSH_ID(button);
            edited |= ImGui::DragFloat("##DragFloat", &val, 0.1f, 0, 0, "%.2f");
         }

         ImGui::PopItemWidth();
      };

      UI_PUSH_STYLE_VAR(ImGuiStyleVar_ItemSpacing, ImVec2{});
      drawFloat(v.x, { 0.8f, 0.1f, 0.15f, 1 }, "X");
      ImGui::SameLine();
      drawFloat(v.y, { 0.2f, 0.7f, 0.2f, 1 }, "Y");
      ImGui::SameLine();
      drawFloat(v.z, { 0.1f, 0.25f, 0.8f, 1 }, "Z");

      if (v != vec3{resetVal}) {
         ImGui::SameLine(0, 10);
         if (ImGui::Button("-")) {
            v = vec3{ resetVal };
            edited = true;
         }
      }

      ImGui::Columns(1);

      return edited;
   }

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

   STRUCT_BEGIN(SceneTransformComponent)
      // todo:
      // STRUCT_FIELD(position)
      // STRUCT_FIELD(rotation)
      // STRUCT_FIELD(scale)
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

   const Transform& SceneTransformComponent::Local() const {
      return local;
   }

   Transform& SceneTransformComponent::Local() {
      return local;
   }

   Transform SceneTransformComponent::World() const {
      Transform transform = local;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         transform = pTrans.World() * transform;
      }
      return transform;
   }

   vec3 SceneTransformComponent::Position() const {
      vec3 pos = local.position;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         pos = pTrans.Position() + pTrans.Rotation() * (pos * pTrans.Scale());
      }
      return pos;
   }

   quat SceneTransformComponent::Rotation() const {
      quat rot = local.rotation;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         rot = pTrans.Rotation() * rot;
      }
      return rot;
   }

   vec3 SceneTransformComponent::Scale() const {
      vec3 s = local.scale;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         s *= pTrans.Scale();
      }
      return s;
   }

   void SceneTransformComponent::SetPosition(const vec3& pos) {
      if (HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         local.position = glm::inverse(pTrans.Rotation()) * (pos - pTrans.Position()) / pTrans.Scale();
      } else {
         local.position = pos;
      }
   }

   void SceneTransformComponent::SetRotation(const quat& rot) {
      if (HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         local.rotation = glm::inverse(pTrans.Rotation()) * rot;
      } else {
         local.rotation = rot;
      }
   }

   void SceneTransformComponent::SetScale(const vec3& s) {
      if (HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         local.scale = s / pTrans.Scale();
      } else {
         local.scale = s;
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

   mat4 SceneTransformComponent::GetWorldMatrix() const {
      return World().GetMatrix();
   }

   mat4 SceneTransformComponent::GetPrevMatrix() const {
      return prevWorld.GetMatrix();
   }

   std::tuple<glm::vec3, glm::quat, glm::vec3> GetTransformDecomposition(const glm::mat4& transform) {
      glm::vec3 scale, translation, skew;
      glm::vec4 perspective;
      glm::quat orientation;
      glm::decompose(transform, scale, orientation, translation, skew, perspective);

      return { translation, orientation, scale };
   }

   SceneTransformComponent::SceneTransformComponent(Entity entity, Entity parent)
         : entity(entity) {
      if (parent) {
         SetParent(parent);
      }
   }

   void SceneTransformComponent::SetMatrix(const mat4& transform) {
      auto [position_, rotation_, scale_] = GetTransformDecomposition(transform);

      // set in world space
      SetPosition(position_);
      SetRotation(rotation_);
      SetScale(scale_);
   }

   void SceneTransformComponent::UpdatePrevTransform() {
      prevWorld = World();
   }

   void SceneTransformComponent::AddChild(Entity child, int iChild, bool keepLocalTransform) {
      child.Get<SceneTransformComponent>().SetParent(entity, iChild, keepLocalTransform);
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

   bool SceneTransformComponent::SetParent(Entity newParent, int iChild, bool keepLocalTransform) {
      if (!newParent) {
         newParent = entity.GetScene()->GetRootEntity();
      }
      ASSERT_MESSAGE(newParent, "New parent must be valid entity");
      return SetParentInternal(newParent, iChild, keepLocalTransform);
   }

   bool SceneTransformComponent::SetParentInternal(Entity newParent, int iChild, bool keepLocalTransform) {
      if (newParent == entity) {
         return false;
      }

      auto pos = Position();
      auto rot = Rotation();
      auto scale = Scale();

      if (HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         if (parent == newParent) {
            int idx = GetChildIdx();
            if (idx < iChild) {
               --iChild;
            } else if (idx == iChild) {
               return false;
            }
         }
         std::erase(pTrans.children, entity);
      }

      parent = newParent;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();

         if (iChild == -1) {
            pTrans.children.push_back(entity);
         } else {
            pTrans.children.insert(pTrans.children.begin() + iChild, entity);
         }
      }

      if (!keepLocalTransform) {
         SetPosition(pos);
         SetRotation(rot);
         SetScale(scale);
      }

      return true;
   }

   int SceneTransformComponent::GetChildIdx() const {
      auto& pTrans = parent.Get<SceneTransformComponent>();
      return (int)std::ranges::distance(pTrans.children.begin(), std::ranges::find(pTrans.children, entity));
   }

   void SceneTransformComponent::Serialize(Serializer& ser) const {
      SERIALIZER_MAP(ser);

      ser.Ser("position", local.position);
      ser.Ser("rotation", local.rotation);
      ser.Ser("scale", local.scale);

      if (HasParent()) {
         ser.KeyValue("parent", parent.Get<UUIDComponent>().uuid);
      }

      if (HasChilds()) {
         ser.Key("children");

         auto& out = ser.out;
         out << YAML::Flow;
         SERIALIZER_SEQ(ser);

         for (auto children : children) {
            out << (uint64)children.GetUUID();
         }
      }
   };

   bool SceneTransformComponent::Deserialize(const Deserializer& deser) {
      // todo:
      RemoveAllChild();

      bool success = true;

      success &= deser.Deser("position", local.position);
      success &= deser.Deser("rotation", local.rotation);
      success &= deser.Deser("scale", local.scale);

      // note: we will be added by our parent
      // auto parent = deser.Deser<uint64>("parent");

      if (auto childrenDeser = deser["children"]) {
         children.reserve(childrenDeser.node.size());

         for (auto child : childrenDeser.node) {
            uint64 childUuid = child.as<uint64>(); // todo: check
            AddChild(entity.GetScene()->GetEntity(childUuid), -1, true);
         }
      }

      return success;
   }

   bool SceneTransformComponent::UI() {
      bool editted = false;

      editted |= Vec3UI("Position", local.position, 0, 70);

      auto degrees = glm::degrees(glm::eulerAngles(local.rotation));
      if (Vec3UI("Rotation", degrees, 0, 70)) {
         local.rotation = glm::radians(degrees);
         editted = true;
      }

      editted |= Vec3UI("Scale", local.scale, 1, 70);

      return editted;
   }

   void RegisterBasicComponents(Typer& typer) {
      // INTERNAL_ADD_COMPONENT(SceneTransformComponent);
      INTERNAL_ADD_COMPONENT(CameraComponent);
      INTERNAL_ADD_COMPONENT(MaterialComponent);
      INTERNAL_ADD_COMPONENT(GeometryComponent);
      INTERNAL_ADD_COMPONENT(LightComponent);
      INTERNAL_ADD_COMPONENT(DirectLightComponent);
      INTERNAL_ADD_COMPONENT(DecalComponent);
      INTERNAL_ADD_COMPONENT(SkyComponent);
      INTERNAL_ADD_COMPONENT(WaterComponent);
      INTERNAL_ADD_COMPONENT(TerrainComponent);
   }

}
