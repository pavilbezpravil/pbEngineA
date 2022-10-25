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
   class Scene;

#define TYPER_BEGIN(type) \
   void TyperRegister_##type() { \
      using CurrentType = type; \
      TypeInfo ti; \
      ti.name = #type; \
      ti.typeID = GetTypeID<type>(); \
      ti.typeSizeOf = sizeof(type); \
      \
      TypeField f{};

   // todo: remove
#define TYPER_FIELD(name) \
      ti.fields.emplace_back(#name, GetTypeID<decltype(CurrentType{}.name)>(), offsetof(CurrentType, name));

   //  for handle initialization like this 'TYPE_FIELD_UI2(UISliderFloat{ .min = -10, .max = 15 })'
   // problem with ','
#define TYPE_FIELD_UI(...) \
      f.uiFunc = __VA_ARGS__;
   // similar to this define
   // #define TYPE_FIELD_UI(func) \
   //       f.uiFunc = func;

#define TYPE_FIELD(_name) \
      f.name = #_name; \
      f.typeID = GetTypeID<decltype(CurrentType{}._name)>(); \
      f.offset = offsetof(CurrentType, _name); \
      ti.fields.emplace_back(f); \
      f = {};

#define TYPER_END(type) \
      Typer::Get().RegisterType(ti.typeID, std::move(ti)); \
   } \
   static TypeRegisterGuard TypeInfo_##type = {TyperRegister_##type, GetTypeID<type>()};

   struct TypeField {
      std::string name;
      TypeID typeID;
      size_t offset;
      std::function<void(const char*, byte*)> uiFunc;
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
      std::function<void(void*, const void*)> duplicate;
   };

   struct NativeScriptInfo {
      using ApplyFunc = std::function<void(class NativeScript&)>;

      TypeID typeID;
      std::function<void(Scene&)> initialize;
      std::function<void(Scene&, const ApplyFunc&)> sceneApplyFunc;
   };

   class CORE_API Typer {
   public:
      Typer();

      static Typer& Get();

      void ImGui();
      void ImGuiTypeInfo(const TypeInfo& ti);

      void RegisterType(TypeID typeID, TypeInfo&& ti);
      void UnregisterType(TypeID typeID);

      void RegisterComponent(ComponentInfo&& ci);
      void UnregisterComponent(TypeID typeID);

      void RegisterNativeScript(NativeScriptInfo&& si);
      void UnregisterNativeScript(TypeID typeID);

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

      std::unordered_map<TypeID, TypeInfo> types;

      std::vector<ComponentInfo> components;
      std::vector<NativeScriptInfo> nativeScripts;
   };

   struct TypeRegisterGuard {
      template<typename Func>
      TypeRegisterGuard(Func f, TypeID typeID) : typeID(typeID) {
         f();
      }
      ~TypeRegisterGuard() {
         Typer::Get().UnregisterType(typeID);
      }
      TypeID typeID;
   };

}
