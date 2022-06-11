#include "Typer.h"
#include <fstream>

#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

#include "imgui.h"
#include "core/Assert.h"
#include "fs/FileSystem.h"


#define TYPER_BEGIN(type) \
   int TyperRegister_##type() { \
      using CurrentType = type; \
      TypeInfo ti; \
      ti.name = #type; \
      ti.typeID = GetTypeID<type>();

#define TYPER_FIELD(name) \
      ti.fields.emplace_back(#name, GetTypeID<decltype(CurrentType{}.name)>(), offsetof(CurrentType, name));

#define TYPER_END(type) \
      sTyper.types[ti.typeID] = ti; \
      return 0; \
   } \
   static int TypeInfo_##type = TyperRegister_##type();


Typer sTyper;


struct Test {
   float f;
   int i;
};

TYPER_BEGIN(Test);
   TYPER_FIELD(f);
   TYPER_FIELD(i);
TYPER_END(Test);

struct EditorShowWindowsConfig {
   bool contentBrowser = true;
   bool viewport = true;
};

TYPER_BEGIN(EditorShowWindowsConfig);
   TYPER_FIELD(contentBrowser);
   TYPER_FIELD(viewport);
TYPER_END(EditorShowWindowsConfig);


struct TestHard {
   Test test;
   int i2;
   bool b;
};

TYPER_BEGIN(TestHard);
   TYPER_FIELD(test);
   TYPER_FIELD(i2);
   TYPER_FIELD(b);
TYPER_END(TestHard);


// int TyperRegister_bool() {
//    TypeInfo ti;
//
//    ti.name = "bool";
//    ti.typeID = GetTypeID<bool>();
//    ti.imguiFunc = [](const char* name, byte* value) { ImGui::Checkbox(name, (bool*)value); };
//    ti.serialize = [](YAML::Node& node, const char* name, byte* value) { node[name] = *(bool*)value; };
//
//    sTyper.types[ti.typeID] = ti;
//    return 0;
// }
// static int TypeInfo_bool = TyperRegister_bool();


Typer::Typer() {
   TypeInfo ti;

   ti = {};
   ti.name = "bool";
   ti.typeID = GetTypeID<bool>();
   ti.imguiFunc = [](const char* name, byte* value) { ImGui::Checkbox(name, (bool*)value); };
   ti.serialize = [](YAML::Node& node, const char* name, const byte* value) { node[name] = *(bool*)value; };
   ti.deserialize = [](YAML::Node& node, const char* name, byte* value) { *(bool*)value = node[name].as<bool>(); };
   types[ti.typeID] = ti;

   ti = {};
   ti.name = "float";
   ti.typeID = GetTypeID<float>();
   ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputFloat(name, (float*)value); };
   ti.serialize = [](YAML::Node& node, const char* name, const byte* value) { node[name] = *(float*)value; };
   ti.deserialize = [](YAML::Node& node, const char* name, byte* value) { *(float*)value = node[name].as<float>(); };
   types[ti.typeID] = ti;

   ti = {};
   ti.name = "int";
   ti.typeID = GetTypeID<int>();
   ti.imguiFunc = [](const char* name, byte* value) { ImGui::InputInt(name, (int*)value); };
   ti.serialize = [](YAML::Node& node, const char* name, const byte* value) { node[name] = *(int*)value; };
   ti.deserialize = [](YAML::Node& node, const char* name, byte* value) { *(int*)value = node[name].as<int>(); };
   types[ti.typeID] = ti;

}


void Typer::ImGui() {
   for (const auto& entry : types) {
      const TypeInfo& ti = entry.second;
      ImGuiTypeInfo(ti);
   }

   ImGui::Separator();

   static int i = 14;
   ImGuiValue("i", i);

   static float f = 245;
   ImGuiValue("f", f);

   static Test test;
   ImGuiValue("test", test);

   static TestHard testHard;
   ImGuiValue("testHard", testHard);

   constexpr const char* testFilename = "test.yaml";

   if (ImGui::Button("test hard save yaml")) {
      YAML::Node node;
      Serialize(node, "testHard", testHard);

      std::ofstream fout{ testFilename };
      fout << node;
   }

   if (ImGui::Button("test hard load yaml")) {
      std::string data = ReadFileAsString(testFilename);
      YAML::Node node = YAML::Load(data);

      // std::ifstream fin{ testFilename };
      // YAML::Node node = YAML::Load(fin);

      Deserialize(node, "testHard", testHard);
   }
}

void Typer::ImGuiTypeInfo(const TypeInfo& ti) {
   if (ImGui::TreeNode(ti.name.c_str())) {
      ImGui::Text("type ID: %llu", ti.typeID);

      if (!ti.fields.empty()) {
         for (const auto& f : ti.fields) {
            if (ImGui::TreeNode(&f, "%s %s", types[f.typeID].name.c_str(), f.name.c_str())) {
               ImGuiTypeInfo(types[f.typeID]);
               ImGui::TreePop();
            }
         }
      }

      ImGui::TreePop();
   }
}

void Typer::ImGuiValueImpl(std::string_view name, TypeID typeID, byte* value) {
   const auto& ti = types[typeID];

   if (ti.imguiFunc) {
      ti.imguiFunc(name.data(), value);
   } else {
      if (ImGui::TreeNode(name.data())) {
         for (const auto& f : ti.fields) {
            byte* data = value + f.offset;
            ImGuiValueImpl(f.name, f.typeID, data);
         }

         ImGui::TreePop();
      }
   }
}

void Typer::SerializeImpl(YAML::Node& node, std::string_view name, TypeID typeID, const byte* value) {
   const auto& ti = types[typeID];

   if (ti.imguiFunc) {
      ti.serialize(node, name.data(), value);
   } else {
      YAML::Node nodeFields;

      for (const auto& f : ti.fields) {
         const byte* data = value + f.offset;
         SerializeImpl(nodeFields, f.name, f.typeID, data);
      }

      node[name.data()] = nodeFields;
   }
}

void Typer::DeserializeImpl(YAML::Node& node, std::string_view name, TypeID typeID, byte* value) {
   const auto& ti = types[typeID];

   if (ti.imguiFunc) {
      ti.deserialize(node, name.data(), value);
   } else {
      if (!node[name.data()]) {
         WARN("Serialization failed! Cant find {}", name);
         return;
      }

      YAML::Node nodeFields = node[name.data()];

      for (const auto& f : ti.fields) {
         byte* data = value + f.offset;
         DeserializeImpl(nodeFields, f.name, f.typeID, data);
      }
   }
}
