#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Core.h"

namespace YAML {
   class Node;
   class Emitter;
}

using TypeID = uint64;

template<typename T>
TypeID GetTypeID() {
   return typeid(T).hash_code(); // todo:
}


struct TypeField {
   std::string name;
   TypeID typeID;
   size_t offset;
};

struct TypeInfo {
   std::string name;
   TypeID typeID;

   std::vector<TypeField> fields;

   std::function<void(const char*, byte*)> imguiFunc;
   std::function<void(YAML::Emitter&, const char*, const byte*)> serialize;
   std::function<void(const YAML::Node&, const char*, byte*)> deserialize;
};

class Typer {
public:
   Typer();

   void ImGui();
   void ImGuiTypeInfo(const TypeInfo& ti);

   template<typename T>
   void ImGuiValue(std::string_view name, T& value) {
      auto typeID = GetTypeID<T>();
      ImGuiValueImpl(name, typeID,(byte*) & value);
   }

   void ImGuiValueImpl(std::string_view name, TypeID typeID, byte* value);

   template<typename T>
   void Serialize(YAML::Emitter& out, std::string_view name, const T& value) {
      auto typeID = GetTypeID<T>();
      SerializeImpl(out, name, typeID, (byte*)&value);
   }

   void SerializeImpl(YAML::Emitter& out, std::string_view name, TypeID typeID, const byte* value);

   template<typename T>
   void Deserialize(const YAML::Node& node, std::string_view name, T& value) {
      auto typeID = GetTypeID<T>();
      DeserializeImpl(node, name, typeID, (byte*)&value);
   }

   void DeserializeImpl(const YAML::Node& node, std::string_view name, TypeID typeID, byte* value);

   std::unordered_map<TypeID, TypeInfo> types;
};


extern Typer sTyper;
