#include "pch.h"
#include "Typer.h"
#include "BasicTypes.h"
#include "Serialize.h"
#include "core/Assert.h"
#include "fs/FileSystem.h"
#include "gui/Gui.h"
#include "scene/Component.h"

namespace pbe {

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
   }

   void Typer::ImGuiTypeInfo(const TypeInfo& ti) {
      if (ImGui::TreeNode(ti.name.c_str())) {
         ImGui::Text("typeID: %llu", ti.typeID);
         ImGui::Text("sizeof: %d", ti.typeSizeOf);
         ImGui::Text("hasEntityRef: %d", ti.hasEntityRef);

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

   TypeInfo& Typer::GetTypeInfo(TypeID typeID) {
      return types.at(typeID);
   }

   bool Typer::UI(std::string_view name, TypeID typeID, byte* value) const {
      const auto& ti = types.at(typeID);

      bool edited = false;

      if (ti.ui) {
         edited = ti.ui(name.data(), value);
      } else {
         bool hasName = !name.empty();

         bool opened = true;
         if (hasName) {
            opened = ImGui::TreeNodeEx(name.data(), ImGuiTreeNodeFlags_AllowOverlap);
         }

         if (opened) {
            for (const auto& f : ti.fields) {
               if (!f.Use(value)) {
                  continue;
               }

               byte* data = value + f.offset;

               auto fieldName = f.Name();
               if (f.ui) {
                  edited |= f.ui(fieldName, data);
               } else {
                  edited |= UI(fieldName, f.typeID, data);
               }
            }
         }

         if (hasName && opened) {
            ImGui::TreePop();
         }
      }

      return edited;
   }

   void Typer::Serialize(Serializer& ser, std::string_view name, TypeID typeID, const byte* value) const {
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
            if (!f.Use(value)) {
               continue;
            }

            const byte* data = value + f.offset;
            Serialize(ser, f.Name(), f.typeID, data);
         }
      }
   }

   bool Typer::Deserialize(const Deserializer& deser, std::string_view name, TypeID typeID, byte* value) const {
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
            // todo: all value that use inside lambda should be deserialized before
            // if (!f.Use(value)) {
            //    continue;
            // }

            byte* data = value + f.offset;
            success &= Deserialize(nodeFields, f.Name(), f.typeID, data);
         }

         return success;
      }
   }

   void Typer::Finalize() {
      std::unordered_set<TypeID> processedTypeIDs;

      for (auto& entry : types) {
         TypeInfo& ti = entry.second;
         ProcessType(ti, processedTypeIDs);
      }
   }

   void Typer::ProcessType(TypeInfo& ti, std::unordered_set<TypeID>& processedTypeIDs) {
      auto processed = processedTypeIDs.find(ti.typeID) != processedTypeIDs.end();
      if (processed) {
         return;
      }

      if (ti.IsSimpleType()) {
         auto entityTypeID = GetTypeID<Entity>();
         ti.hasEntityRef = ti.typeID == entityTypeID;
      } else {
         for (const auto& field : ti.fields) {
            auto& filedTypeInfo = GetTypeInfo(field.typeID);
            ProcessType(filedTypeInfo, processedTypeIDs);

            if (filedTypeInfo.hasEntityRef) {
               ti.hasEntityRef = true;
               break;
            }
         }
      }

      processedTypeIDs.insert(ti.typeID);
   }

}
