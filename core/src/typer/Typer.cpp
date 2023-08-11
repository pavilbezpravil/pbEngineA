#include "pch.h"
#include "Typer.h"
#include "BasicTypes.h"
#include "Serialize.h"
#include "core/Assert.h"
#include "fs/FileSystem.h"
#include "gui/Gui.h"
#include "scene/Component.h"

namespace pbe {

   struct Test {
      float f;
      int i;
   };

   TYPER_BEGIN(Test);
      TYPER_FIELD(f);
      TYPER_FIELD(i);
   TYPER_END();

   struct TestHard {
      Test test;
      int i2;
      bool b;
   };

   TYPER_BEGIN(TestHard);
      TYPER_FIELD(test);
      TYPER_FIELD(i2);
      TYPER_FIELD(b);
   TYPER_END();

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
      RegisterBasicComponents(*this);
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
         Serializer ser;
         {
            SERIALIZER_MAP(ser);
            ser.Ser("i", i);
            ser.Ser("f", f);
            ser.Ser("test", test);
            ser.Ser("testHard", testHard);
         }
         ser.SaveToFile(testFilename);
      }

      if (ImGui::Button("test hard load yaml")) {
         // std::string data = ReadFileAsString(testFilename);
         // YAML::Node node = YAML::Load(data);

         auto deser = Deserializer::FromFile(testFilename);
         deser.Deser("i", i);
         deser.Deser("f", f);
         deser.Deser("test", test);
         deser.Deser("testHard", testHard);
      }

      ImGui::Separator();
      if (UI_TREE_NODE("Read file", ImGuiTreeNodeFlags_SpanFullWidth)) {
         static std::string filename = testFilename;
         filename.reserve(256);
         ImGui::InputText("filename", filename.data(), filename.capacity());

         std::string data = ReadFileAsString(filename.c_str());
         ImGui::Text("%s", data.c_str());
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

   void Typer::UnregisterType(TypeID typeID) {
      types.erase(typeID);
   }

   void Typer::RegisterComponent(ComponentInfo&& ci) {
      auto it = std::ranges::find(components, ci.typeID, &ComponentInfo::typeID);
      ASSERT(it == components.end());
      components.emplace_back(std::forward<ComponentInfo>(ci));
   }

   void Typer::UnregisterComponent(TypeID typeID) {
      auto it = std::ranges::find(components, typeID, &ComponentInfo::typeID);
      components.erase(it);
   }

   void Typer::RegisterScript(ScriptInfo&& si) {
      auto it = std::ranges::find(scripts, si.typeID, &ScriptInfo::typeID);
      ASSERT(it == scripts.end());
      scripts.emplace_back(std::move(si));
   }

   void Typer::UnregisterScript(TypeID typeID) {
      auto it = std::ranges::find(scripts, typeID, &ScriptInfo::typeID);
      scripts.erase(it);
   }

   const TypeInfo& Typer::GetTypeInfo(TypeID typeID) const {
      return types.at(typeID);
   }

   TypeInfo& Typer::GetTypeInfo(TypeID typeID)
   {
      return types.at(typeID);
   }

   bool Typer::ImGuiValueImpl(std::string_view name, TypeID typeID, byte* value) const {
      const auto& ti = types.at(typeID);

      bool edited = false;

      if (ti.imguiFunc) {
         edited = ti.imguiFunc(name.data(), value);
      } else {
         bool hasName = !name.empty();

         bool opened = true;
         if (hasName) {
            opened = ImGui::TreeNodeEx(name.data(), DefaultTreeNodeFlags());
         }

         if (opened) {
            for (const auto& f : ti.fields) {
               byte* data = value + f.offset;
               if (f.uiFunc) {
                  edited |= f.uiFunc(f.name.c_str(), data);
               } else {
                  edited |= ImGuiValueImpl(f.name, f.typeID, data);
               }
            }
         }

         if (hasName && opened) {
            ImGui::TreePop();
         }
      }

      return edited;
   }

   void Typer::SerializeImpl(Serializer& ser, std::string_view name, TypeID typeID, const byte* value) const {
      ASSERT(types.find(typeID) != types.end());
      const auto& ti = types.at(typeID);

      if (!name.empty()) {
         ser.out << YAML::Key << name.data() << YAML::Value;
      }

      if (ti.serialize) {
         ti.serialize(ser, value);
      } else {
         SERIALIZER_MAP(ser);

         for (const auto& f : ti.fields) {
            const byte* data = value + f.offset;
            SerializeImpl(ser, f.name, f.typeID, data);
         }
      }
   }

   bool Typer::DeserializeImpl(const Deserializer& deser, std::string_view name, TypeID typeID, byte* value) const {
      bool hasName = !name.empty();

      if (hasName && !deser[name.data()]) {
         WARN("Serialization failed! Cant find {}", name);
         return false;
      }

      const auto& ti = types.at(typeID);

      const Deserializer& nodeFields = hasName ? deser[name.data()] : deser;

      if (ti.deserialize) {
         return ti.deserialize(nodeFields, value);
      } else {
         bool success = true;

         for (const auto& f : ti.fields) {
            byte* data = value + f.offset;
            success &= DeserializeImpl(nodeFields, f.name, f.typeID, data);
         }

         return success;
      }
   }

}
