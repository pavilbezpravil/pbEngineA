#include "pch.h"
#include "BasicTypes.h" // todo:
#include "Typer.h"
#include "gui/Gui.h"
#include "math/Types.h"
#include "scene/Component.h"
#include "scene/Entity.h"
#include "typer/Serialize.h"


namespace pbe {

   static void SerVec3(Serializer& ser, const vec3& v) {
      ser.out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
   }

   static bool DeserVec3(const Deserializer& deser, vec3& v) {
      if (!deser.node.IsSequence() || deser.node.size() != 3) {
         return false;
      }

      v.x = deser.node[0].as<float>();
      v.y = deser.node[1].as<float>();
      v.z = deser.node[2].as<float>();
      return true;
   }

#define START_DECL_TYPE(Type) \
   ti = {}; \
   ti.name = STRINGIFY(Type); \
   ti.typeID = GetTypeID<Type>(); \
   ti.typeSizeOf = sizeof(Type)

#define DEFAULT_SER_DESER(Type) \
   ti.serialize = [](Serializer& ser, const byte* value) { ser.out << *(Type*)value; }; \
   ti.deserialize = [](const Deserializer& deser, byte* value) { return YAML::convert<Type>::decode(deser.node, *(Type*)value); };

#define END_DECL_TYPE() \
   typer.RegisterType(ti.typeID, std::move(ti))

   void RegisterBasicTypes(Typer& typer) {
      TypeInfo ti;

      START_DECL_TYPE(bool);
      ti.ui = [](const char* name, byte* value) { return ImGui::Checkbox(name, (bool*)value); };
      DEFAULT_SER_DESER(bool);
      END_DECL_TYPE();

      START_DECL_TYPE(float);
      ti.ui = [](const char* name, byte* value) { return ImGui::InputFloat(name, (float*)value); };
      DEFAULT_SER_DESER(float);
      END_DECL_TYPE();

      START_DECL_TYPE(int);
      ti.ui = [](const char* name, byte* value) { return ImGui::InputInt(name, (int*)value); };
      DEFAULT_SER_DESER(int);
      END_DECL_TYPE();

      START_DECL_TYPE(int64);
      ti.ui = [](const char* name, byte* value) {
         // todo:
         const char* format = "%d";
         return ImGui::InputScalar(name, ImGuiDataType_S64, value, NULL, NULL, format, 0);
         // ImGui::InputInt(name, (int*)value);
      };
      DEFAULT_SER_DESER(int64);
      END_DECL_TYPE();

      START_DECL_TYPE(uint64);
      ti.ui = [](const char* name, byte* value) {
         const char* format = "%d";
         return ImGui::InputScalar(name, ImGuiDataType_U64, value, NULL, NULL, format, 0);
      };
      DEFAULT_SER_DESER(uint64);
      END_DECL_TYPE();

      START_DECL_TYPE(string);
      // todo:
      ti.ui = [](const char* name, byte* value) { ImGui::Text(((string*)value)->data()); return false; };
      DEFAULT_SER_DESER(string);
      END_DECL_TYPE();

      START_DECL_TYPE(vec2);
      ti.ui = [](const char* name, byte* value) { return ImGui::InputFloat2(name, (float*)value); };
      ti.serialize = [](Serializer& ser, const byte* value){
         const auto& v = *(vec2*)value;
         ser.out << YAML::Flow << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
      };
      ti.deserialize = [](const Deserializer& deser, byte* value) {
         if (!deser.node.IsSequence() || deser.node.size() != 2) {
            return false;
         }

         auto& v = *(vec2*)value;
         v.x = deser.node[0].as<float>();
         v.y = deser.node[1].as<float>();
         return true;
      };
      END_DECL_TYPE();

      START_DECL_TYPE(vec3);
      ti.ui = [](const char* name, byte* value) { return ImGui::InputFloat3(name, (float*)value); };
      ti.serialize = [](Serializer& ser, const byte* value) {
         SerVec3(ser, *(vec3*)value);
      };
      ti.deserialize = [](const Deserializer& deser, byte* value) {
         return DeserVec3(deser, *(vec3*)value);
      };
      END_DECL_TYPE();

      START_DECL_TYPE(vec4);
      ti.ui = [](const char* name, byte* value) { return ImGui::ColorEdit4(name, (float*)value); };
      ti.serialize = [](Serializer& ser, const byte* value) {
         const auto& v = *(vec4*)value;
         ser.out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
      };
      ti.deserialize = [](const Deserializer& deser, byte* value) {
         if (!deser.node.IsSequence() || deser.node.size() != 4) {
            return false;
         }

         auto& v = *(vec4*)value;
         v.x = deser.node[0].as<float>();
         v.y = deser.node[1].as<float>();
         v.z = deser.node[2].as<float>();
         v.w = deser.node[3].as<float>();
         return true;
      };
      END_DECL_TYPE();

      START_DECL_TYPE(quat);
      ti.ui = [](const char* name, byte* value) {
         auto angles = glm::degrees(glm::eulerAngles(*(quat*)value));
         if (ImGui::InputFloat3(name, &angles.x)) {
            *(quat*)value = glm::radians(angles);
            return true;
         }
         return false;
      };
      ti.serialize = [](Serializer& ser, const byte* value) {
         auto angles = glm::degrees(glm::eulerAngles(*(quat*)value));
         SerVec3(ser, angles);
      };
      ti.deserialize = [](const Deserializer& deser, byte* value) {
         vec3 angles;
         if (!DeserVec3(deser, angles)) {
            return false;
         }
         *(quat*)value = glm::quat{ glm::radians(angles) };
         return true;
      };
      END_DECL_TYPE();

      // todo: it is not basic type
      START_DECL_TYPE(Entity);
      ti.ui = [](const char* name, byte* value) {
         Entity* e = (Entity*)value;

         ImGui::Text(name);
         ImGui::SameLine();

         const char* entityName = e->Valid() ? e->GetName() : "None";
         if (ImGui::Button(entityName)) {
            
         }

         if (ui::DragDropTarget ddTarget{ DRAG_DROP_ENTITY }) {
            *e = *ddTarget.GetPayload<Entity>();
            return true;
         }

         return false;
      };
      ti.serialize = [](Serializer& ser, const byte* value) {
         const auto& e = *(Entity*)value;

         if (e.Valid()) {
            ser.out << (pbe::uint64)e.GetUUID();
         } else {
            ser.out << (pbe::uint64)entt::null;
         }
      };
      ti.deserialize = [](const Deserializer& deser, byte* value) {
         pbe::UUID entityUUID = deser.node.as<pbe::uint64>();
         if ((pbe::uint64)entityUUID != (pbe::uint64)entt::null) {
            pbe::Scene* scene = pbe::Scene::GetCurrentDeserializedScene();
            *(Entity*)value = scene->GetEntity(entityUUID);
         }

         return true;
      };
      END_DECL_TYPE();
   }

}
