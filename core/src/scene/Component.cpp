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
            v = { resetVal };
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

   TYPER_BEGIN(TagComponent)
      TYPER_FIELD(tag)
   TYPER_END()

   TYPER_BEGIN(CameraComponent)
      TYPER_FIELD(main)
   TYPER_END()

   TYPER_BEGIN(SceneTransformComponent)
      TYPER_FIELD(position)
      TYPER_FIELD(rotation)
      TYPER_FIELD(scale)
   TYPER_END()

   TYPER_BEGIN(MaterialComponent)
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
      TYPER_DESERIALIZE([](const Deserializer& deser, byte* value) { *(int*)value = deser.node.as<int>(); return true; }) // todo:
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

   TYPER_BEGIN(RigidBodyComponent)
      TYPER_FIELD(dynamic)
   TYPER_END()

   TYPER_BEGIN(TriggerComponent)
   TYPER_END()

   TYPER_BEGIN(DistanceJointComponent)
      TYPER_FIELD(entity0)
      TYPER_FIELD(entity1)
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

      ser.Ser("position", position);
      ser.Ser("rotation", rotation);
      ser.Ser("scale", scale);

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

      success &= deser.Deser("position", position);
      success &= deser.Deser("rotation", rotation);
      success &= deser.Deser("scale", scale);

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

      editted |= Vec3UI("Position", position, 0, 70);

      auto degrees = glm::degrees(glm::eulerAngles(rotation));
      if (Vec3UI("Rotation", degrees, 0, 70)) {
         rotation = glm::radians(degrees);
         editted = true;
      }

      editted |= Vec3UI("Scale", scale, 1, 70);

      return editted;
   }

   void RigidBodyComponent::SetLinearVelocity(const vec3& v, bool autowake) {
      // todo:
      auto dynamic = pxRigidActor->is<physx::PxRigidDynamic>();
      dynamic->setLinearVelocity(Vec3ToPx(v), autowake);
   }

   void RigidBodyComponent::OnChanged() {
      INFO(__FUNCDNAME__);
   }

   void RegisterBasicComponents(Typer& typer) {
      // INTERNAL_ADD_COMPONENT(SceneTransformComponent);
      INTERNAL_ADD_COMPONENT(CameraComponent);
      INTERNAL_ADD_COMPONENT(MaterialComponent);
      INTERNAL_ADD_COMPONENT(GeometryComponent);
      INTERNAL_ADD_COMPONENT(RigidBodyComponent);
      INTERNAL_ADD_COMPONENT(TriggerComponent);
      INTERNAL_ADD_COMPONENT(DistanceJointComponent);
      INTERNAL_ADD_COMPONENT(LightComponent);
      INTERNAL_ADD_COMPONENT(DirectLightComponent);
      INTERNAL_ADD_COMPONENT(DecalComponent);
      INTERNAL_ADD_COMPONENT(SkyComponent);
      INTERNAL_ADD_COMPONENT(WaterComponent);
      INTERNAL_ADD_COMPONENT(TerrainComponent);
   }

}
