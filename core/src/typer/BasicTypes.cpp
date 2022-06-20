#include "BasicTypes.h"
#include "Typer.h"
#include "math/Types.h"

#include "imgui.h"

#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"


#define START_DECL_TYPE(Type) \
   ti = {}; \
   ti.name = STRINGIFY(Type); \
   ti.typeID = GetTypeID<Type>(); \
   ti.typeSizeOf = sizeof(Type)

#define END_DECL_TYPE() \
   typer.RegisterType(ti.typeID, std::move(ti))

void RegisterBasicTypes(Typer& typer) {
   TypeInfo ti;

   START_DECL_TYPE(bool);
   ti.imguiFunc = [](const char* name, byte* value) { ImGui::Checkbox(name, (bool*)value); };
   ti.serialize = [](YAML::Emitter& emitter, const char* name, const byte* value) { emitter << YAML::Key << name << YAML::Value << *(bool*)value; };
   ti.deserialize = [](const YAML::Node& node, const char* name, byte* value) { *(bool*)value = node[name].as<bool>(); };
   END_DECL_TYPE();

   START_DECL_TYPE(float);
   ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputFloat(name, (float*)value); };
   ti.serialize = [](YAML::Emitter& emitter, const char* name, const byte* value) { emitter << YAML::Key << name << YAML::Value << *(float*)value; };
   ti.deserialize = [](const YAML::Node& node, const char* name, byte* value) { *(float*)value = node[name].as<float>(); };
   END_DECL_TYPE();

   START_DECL_TYPE(int);
   ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputInt(name, (int*)value); };
   ti.serialize = [](YAML::Emitter& emitter, const char* name, const byte* value) { emitter << YAML::Key << name << YAML::Value << *(int*)value; };
   ti.deserialize = [](const YAML::Node& node, const char* name, byte* value) { *(int*)value = node[name].as<int>(); };
   END_DECL_TYPE();

   START_DECL_TYPE(vec3);
   ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputFloat3(name, (float*)value); };
   // ti.serialize = [](YAML::Emitter& emitter, const char* name, const byte* value) { emitter << YAML::Key << name << YAML::Value << *(int*)value; };
   // ti.deserialize = [](const YAML::Node& node, const char* name, byte* value) { *(int*)value = node[name].as<int>(); };
   END_DECL_TYPE();

   START_DECL_TYPE(quat);
   ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputFloat4(name, (float*)value); };
   // ti.serialize = [](YAML::Emitter& emitter, const char* name, const byte* value) { emitter << YAML::Key << name << YAML::Value << *(int*)value; };
   // ti.deserialize = [](const YAML::Node& node, const char* name, byte* value) { *(int*)value = node[name].as<int>(); };
   END_DECL_TYPE();
}
