#include "pch.h"
#include "SceneTransform.h"

#include "Component.h"
#include "typer/Typer.h"
#include "scene/Entity.h"
#include "imgui_internal.h"
#include "gui/Gui.h"
#include "math/Common.h"
#include "math/Random.h"
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

      auto drawFloat = [&](float& val, const ImVec4& color, const char* button) {
         {
            UI_PUSH_STYLE_COLOR(ImGuiCol_Button, color);
            UI_PUSH_STYLE_COLOR(ImGuiCol_ButtonHovered, (color + ImVec4{ 0.1f, 0.1f, 0.1f, 1 }));
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

      if (v != vec3{ resetVal }) {
         ImGui::SameLine(0, 10);
         if (ImGui::Button("-")) {
            v = vec3{ resetVal };
            edited = true;
         }
      }

      ImGui::Columns(1);

      return edited;
   }

   STRUCT_BEGIN(SceneTransformComponent)
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

   vec3 SceneTransformComponent::Position(Space space) const {
      if (space == Space::Local) {
         return local.position;
      }

      vec3 pos = local.position;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         pos = pTrans.Position() + pTrans.Rotation() * (pos * pTrans.Scale());
      }
      return pos;
   }

   quat SceneTransformComponent::Rotation(Space space) const {
      if (space == Space::Local) {
         return local.rotation;
      }

      quat rot = local.rotation;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         rot = pTrans.Rotation() * rot;
      }
      return rot;
   }

   vec3 SceneTransformComponent::Scale(Space space) const {
      if (space == Space::Local) {
         return local.scale;
      }

      vec3 s = local.scale;
      if (parent) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         s *= pTrans.Scale();
      }
      return s;
   }

   SceneTransformComponent& SceneTransformComponent::SetTransform(const Transform& transform, Space space) {
      if (space == Space::World && HasParent()) {
         auto pTrans = parent.Get<SceneTransformComponent>().World();
         local = pTrans.TransformInv(transform);
      } else {
         local = transform;
      }
      return *this;
   }

   SceneTransformComponent& SceneTransformComponent::SetPosition(const vec3& pos, Space space) {
      if (space == Space::World && HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         local.position = glm::inverse(pTrans.Rotation()) * (pos - pTrans.Position()) / pTrans.Scale();
      } else {
         local.position = pos;
      }
      return *this;
   }

   SceneTransformComponent& SceneTransformComponent::SetRotation(const quat& rot, Space space) {
      if (space == Space::World && HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         local.rotation = glm::inverse(pTrans.Rotation()) * rot;
      } else {
         local.rotation = rot;
      }
      return *this;
   }

   SceneTransformComponent& SceneTransformComponent::SetScale(const vec3& s, Space space) {
      if (space == Space::World && HasParent()) {
         auto& pTrans = parent.Get<SceneTransformComponent>();
         local.scale = s / pTrans.Scale();
      } else {
         local.scale = s;
      }
      return *this;
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

   SceneTransformComponent& SceneTransformComponent::AddChild(Entity child, int iChild, bool keepLocalTransform) {
      child.Get<SceneTransformComponent>().SetParent(entity, iChild, keepLocalTransform);
      return *this;
   }

   SceneTransformComponent& SceneTransformComponent::RemoveChild(int idx) {
      ASSERT(idx >= 0 && idx < children.size());
      children[idx].Get<SceneTransformComponent>().SetParent(); // todo: mb set to scene root?
      return *this;
   }

   SceneTransformComponent& SceneTransformComponent::RemoveAllChild(Entity theirNewParent) {
      for (int i = (int)children.size() - 1; i >= 0; --i) {
         children[i].Get<SceneTransformComponent>().SetParent(theirNewParent);
      }
      ASSERT(!HasChilds());
      return *this;
   }

   SceneTransformComponent& SceneTransformComponent::SetParent(Entity newParent, int iChild, bool keepLocalTransform) {
      if (!newParent) {
         newParent = entity.GetScene()->GetRootEntity();
      }
      ASSERT_MESSAGE(newParent, "New parent must be valid entity");
      return SetParentInternal(newParent, iChild, keepLocalTransform);
   }

   SceneTransformComponent& SceneTransformComponent::SetParentInternal(Entity newParent, int iChild, bool keepLocalTransform) {
      if (newParent == entity) {
         return *this;
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
               return *this;
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

      return *this;
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

}
