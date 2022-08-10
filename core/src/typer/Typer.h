#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Core.h"
#include "core/Type.h"

namespace YAML {
   class Node;
   class Emitter;
}

namespace pbe {
   class Entity;

#define TYPER_BEGIN(type) \
   int TyperRegister_##type() { \
      using CurrentType = type; \
      TypeInfo ti; \
      ti.name = #type; \
      ti.typeID = GetTypeID<type>(); \
      ti.typeSizeOf = sizeof(type);

#define TYPER_FIELD(name) \
      ti.fields.emplace_back(#name, GetTypeID<decltype(CurrentType{}.name)>(), offsetof(CurrentType, name));

#define TYPER_END(type) \
      Typer::Get().RegisterType(ti.typeID, std::move(ti)); \
      return 0; \
   } \
   static int TypeInfo_##type = TyperRegister_##type();

   struct TypeField {
      std::string name;
      TypeID typeID;
      size_t offset;
   };

   struct TypeInfo {
      std::string name;
      TypeID typeID;
      int typeSizeOf;

      std::vector<TypeField> fields;

      std::function<void(const char*, byte*)> imguiFunc;
      std::function<void(YAML::Emitter&, const char*, const byte*)> serialize;
      std::function<void(const YAML::Node&, const char*, byte*)> deserialize;
   };

   struct ComponentInfo {
      TypeID typeID;
      std::function<void*(Entity&)> tryGet;
      std::function<void*(Entity&)> getOrAdd;
   };

   class Typer {
   public:
      Typer();

      static Typer& Get();

      void ImGui();
      void ImGuiTypeInfo(const TypeInfo& ti);

      void RegisterType(TypeID typeID, TypeInfo&& ti);
      const TypeInfo& GetTypeInfo(TypeID typeID) const;

      template<typename T>
      void ImGuiValue(std::string_view name, T& value) const {
         auto typeID = GetTypeID<T>();
         ImGuiValueImpl(name, typeID, (byte*)&value);
      }

      void ImGuiValueImpl(std::string_view name, TypeID typeID, byte* value) const;

      template<typename T>
      void Serialize(YAML::Emitter& out, std::string_view name, const T& value) const {
         auto typeID = GetTypeID<T>();
         SerializeImpl(out, name, typeID, (byte*)&value);
      }

      void SerializeImpl(YAML::Emitter& out, std::string_view name, TypeID typeID, const byte* value) const;

      template<typename T>
      void Deserialize(const YAML::Node& node, std::string_view name, T& value) const {
         auto typeID = GetTypeID<T>();
         DeserializeImpl(node, name, typeID, (byte*)&value);
      }

      void DeserializeImpl(const YAML::Node& node, std::string_view name, TypeID typeID, byte* value) const;

      void RegisterComponent(ComponentInfo&& ci);

      std::unordered_map<TypeID, TypeInfo> types;

      std::vector<ComponentInfo> components;
   };

}
