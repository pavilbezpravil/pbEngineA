#include "pch.h"
#include "BasicTypes.h" // todo:
#include "Typer.h"
#include "gui/Gui.h"
#include "math/Types.h"
#include "scene/Component.h"
#include "scene/Entity.h"

namespace YAML {
   template<>
   struct convert<glm::vec3> {
      // static Node encode(const glm::vec3& rhs) {
      //    Node node;
      //    node.push_back(rhs.x);
      //    node.push_back(rhs.y);
      //    node.push_back(rhs.z);
      //    return node;
      // }
   
      static bool decode(const Node& node, glm::vec3& rhs) {
         if (!node.IsSequence() || node.size() != 3) {
            return false;
         }
   
         rhs.x = node[0].as<float>();
         rhs.y = node[1].as<float>();
         rhs.z = node[2].as<float>();
         return true;
      }
   };

   YAML::Emitter& operator << (YAML::Emitter& out, const glm::vec3& v) {
      out << YAML::Flow;
      out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
      return out;
   }

   template<>
   struct convert<glm::vec4> {
      // static Node encode(const glm::vec3& rhs) {
      //    Node node;
      //    node.push_back(rhs.x);
      //    node.push_back(rhs.y);
      //    node.push_back(rhs.z);
      //    return node;
      // }

      static bool decode(const Node& node, glm::vec4& rhs) {
         if (!node.IsSequence() || node.size() != 4) {
            return false;
         }

         rhs.x = node[0].as<float>();
         rhs.y = node[1].as<float>();
         rhs.z = node[2].as<float>();
         rhs.w = node[3].as<float>();
         return true;
      }
   };

   YAML::Emitter& operator << (YAML::Emitter& out, const glm::vec4& v) {
      out << YAML::Flow;
      out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
      return out;
   }

   template<>
   struct convert<glm::quat> {
      // static Node encode(const glm::quat& rhs) {
      //    Node node;
      //    node.push_back(rhs.x);
      //    node.push_back(rhs.y);
      //    node.push_back(rhs.z);
      //    node.push_back(rhs.w);
      //    return node;
      // }

      static bool decode(const Node& node, glm::quat& rhs) {
         pbe::vec3 degrees = glm::radians(node.as<glm::vec3>());
         rhs = glm::quat{degrees};

         return true;
      }
   };

   YAML::Emitter& operator << (YAML::Emitter& out, const glm::quat& v) {
      auto angles = glm::degrees(glm::eulerAngles(v));
      out << angles;
      return out;
   }

   template<>
   struct convert<pbe::Entity> {
      static bool decode(const Node& node, pbe::Entity& rhs) {
         pbe::UUID entityUUID = node.as<pbe::uint64>();
         if ((pbe::uint64)entityUUID != (pbe::uint64)entt::null) {
            pbe::Scene* scene = pbe::Scene::GetCurrentDeserializedScene();
            rhs = scene->GetEntity(entityUUID);
         }

         return true;
      }
   };

   YAML::Emitter& operator << (YAML::Emitter& out, const pbe::Entity& v) {
      if (v.Valid()) {
         out << (pbe::uint64)v.Get<pbe::UUIDComponent>().uuid;
      } else {
         out << (pbe::uint64)entt::null;
      }

      return out;
   }
}

namespace pbe {

#define START_DECL_TYPE(Type) \
   ti = {}; \
   ti.name = STRINGIFY(Type); \
   ti.typeID = GetTypeID<Type>(); \
   ti.typeSizeOf = sizeof(Type)

#define DEFAULT_SER_DESER(Type) \
   ti.serialize = [](YAML::Emitter& emitter, const char* name, const byte* value) { emitter << YAML::Key << name << YAML::Value << *(Type*)value; }; \
   ti.deserialize = [](const YAML::Node& node, const char* name, byte* value) { *(Type*)value = node[name].as<Type>(); };

#define END_DECL_TYPE() \
   typer.RegisterType(ti.typeID, std::move(ti))

   void RegisterBasicTypes(Typer& typer) {
      TypeInfo ti;

      START_DECL_TYPE(bool);
      ti.imguiFunc = [](const char* name, byte* value) { ImGui::Checkbox(name, (bool*)value); };
      DEFAULT_SER_DESER(bool);
      END_DECL_TYPE();

      START_DECL_TYPE(float);
      ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputFloat(name, (float*)value); };
      DEFAULT_SER_DESER(float);
      END_DECL_TYPE();

      START_DECL_TYPE(int);
      ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputInt(name, (int*)value); };
      DEFAULT_SER_DESER(int);
      END_DECL_TYPE();

      START_DECL_TYPE(int64);
      ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputInt(name, (int*)value); };
      DEFAULT_SER_DESER(int64);
      END_DECL_TYPE();

      START_DECL_TYPE(string);
      // todo:
      ti.imguiFunc = [](const char* name, byte* value) { ImGui::Text(((string*)value)->data()); };
      DEFAULT_SER_DESER(string);
      END_DECL_TYPE();

      START_DECL_TYPE(vec3);
      ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputFloat3(name, (float*)value); };
      DEFAULT_SER_DESER(vec3);
      END_DECL_TYPE();

      START_DECL_TYPE(vec4);
      ti.imguiFunc = [](const char* name, byte* value) { ImGui::ColorEdit4(name, (float*)value); };
      DEFAULT_SER_DESER(vec4);
      END_DECL_TYPE();

      START_DECL_TYPE(quat);
      ti.imguiFunc = [](const char* name, byte* value) {
         auto angles = glm::degrees(glm::eulerAngles(*(quat*)value));
         if (ImGui::InputFloat3(name, &angles.x)) {
            *(quat*)value = glm::radians(angles);
         }
      };
      DEFAULT_SER_DESER(quat);
      END_DECL_TYPE();

      START_DECL_TYPE(Entity);
      ti.imguiFunc = [](const char* name, byte* value) {
         Entity* e = (Entity*)value;

         ImGui::Text(name);
         ImGui::SameLine();

         const char* entityName = e->Valid() ? e->Get<TagComponent>().tag.c_str() : "None";
         if (ImGui::Button(entityName)) {
            
         }

         if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAG_DROP_ENTITY)) {
               ASSERT(payload->DataSize == sizeof(Entity));
               Entity ddEntity = *(Entity*)payload->Data;
               *e = ddEntity;
            }
            ImGui::EndDragDropTarget();
         }
      };
      DEFAULT_SER_DESER(Entity);
      END_DECL_TYPE();
   }

}
