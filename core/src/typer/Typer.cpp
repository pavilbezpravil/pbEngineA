#include "Typer.h"
#include <fstream>
#include "BasicTypes.h"
#include "imgui.h"
#include "core/Assert.h"
#include "fs/FileSystem.h"

// todo:
#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"


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
   RegisterBasicTypes(*this);
}


Typer& Typer::Get() {
   static Typer sTyper;
   return sTyper;
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
      YAML::Emitter out;

      out << YAML::BeginMap;
      Serialize(out, "i", i);
      Serialize(out, "f", f);
      Serialize(out, "test", test);
      Serialize(out, "testHard", testHard);
      out << YAML::EndMap;

      std::ofstream fout{ testFilename };
      fout << out.c_str();
   }

   if (ImGui::Button("test hard load yaml")) {
      // std::string data = ReadFileAsString(testFilename);
      // YAML::Node node = YAML::Load(data);

      std::ifstream fin{ testFilename };
      YAML::Node node = YAML::Load(fin);

      Deserialize(node, "i", i);
      Deserialize(node, "f", f);
      Deserialize(node, "test", test);
      Deserialize(node, "testHard", testHard);
   }
}

void Typer::ImGuiTypeInfo(const TypeInfo& ti) {
   if (ImGui::TreeNode(ti.name.c_str())) {
      ImGui::Text("typeID: %llu", ti.typeID);
      ImGui::Text("sizeof: %d", ti.typeSizeOf);

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

void Typer::RegisterType(TypeID typeID, TypeInfo&& ti) {
   ASSERT(types.find(typeID) == types.end());
   types[typeID] = std::move(ti);
}

void Typer::ImGuiValueImpl(std::string_view name, TypeID typeID, byte* value) {
   const auto& ti = types[typeID];

   if (ti.imguiFunc) {
      ti.imguiFunc(name.data(), value);
   } else {
      if (ImGui::TreeNodeEx(name.data(), ImGuiTreeNodeFlags_SpanFullWidth)) {
         for (const auto& f : ti.fields) {
            byte* data = value + f.offset;
            ImGuiValueImpl(f.name, f.typeID, data);
         }

         ImGui::TreePop();
      }
   }
}

void Typer::SerializeImpl(YAML::Emitter& out, std::string_view name, TypeID typeID, const byte* value) {
   const auto& ti = types[typeID];

   if (ti.imguiFunc) {
      ti.serialize(out, name.data(), value);
   } else {
      out << YAML::Key << name.data() << YAML::Value;

      out << YAML::BeginMap;

      for (const auto& f : ti.fields) {
         const byte* data = value + f.offset;
         SerializeImpl(out, f.name, f.typeID, data);
      }

      out << YAML::EndMap;
   }
}

void Typer::DeserializeImpl(const YAML::Node& node, std::string_view name, TypeID typeID, byte* value) {
   const auto& ti = types[typeID];

   if (!node[name.data()]) {
      WARN("Serialization failed! Cant find {}", name);
      return;
   }

   if (ti.imguiFunc) {
      ti.deserialize(node, name.data(), value);
   } else {
      const YAML::Node& nodeFields = node[name.data()];

      for (const auto& f : ti.fields) {
         byte* data = value + f.offset;
         DeserializeImpl(nodeFields, f.name, f.typeID, data);
      }
   }
}
