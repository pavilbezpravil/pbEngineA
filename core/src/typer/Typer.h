#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Common.h"
#include "core/Core.h"
#include "core/Type.h"


namespace pbe {
   struct Serializer;
   struct Deserializer;

   class Entity;
   class Scene;

#define TYPER_BEGIN(type) \
   static TypeRegisterGuard TypeRegisterGuard_##type = {GetTypeID<type>(), \
         [] () { \
      using CurrentType = type; \
      TypeInfo ti; \
      ti.name = #type; \
      ti.typeID = GetTypeID<type>(); \
      ti.typeSizeOf = sizeof(type); \
      \
      TypeField f{};

#define TYPER_SERIALIZE(...) \
      ti.serialize = __VA_ARGS__;

#define TYPER_DESERIALIZE(...) \
      ti.deserialize = __VA_ARGS__;

#define TYPER_UI(...) \
      ti.imguiFunc = __VA_ARGS__;

   //  for handle initialization like this 'TYPER_FIELD_UI2(UISliderFloat{ .min = -10, .max = 15 })'
   // problem with ','
#define TYPER_FIELD_UI(...) \
      f.uiFunc = __VA_ARGS__;
   // similar to this define
   // #define TYPER_FIELD_UI(func) \
   //       f.uiFunc = func;

#define TYPER_FIELD(_name) \
      f.name = #_name; \
      f.typeID = GetTypeID<decltype(CurrentType{}._name)>(); \
      f.offset = offsetof(CurrentType, _name); \
      ti.fields.emplace_back(f); \
      f = {};

#define TYPER_END() \
      Typer::Get().RegisterType(ti.typeID, std::move(ti)); \
   }};

   struct TypeField {
      std::string name;
      TypeID typeID;
      size_t offset;
      std::function<bool(const char*, byte*)> uiFunc;
   };

   struct TypeInfo {
      std::string name;
      TypeID typeID;
      int typeSizeOf;

      std::vector<TypeField> fields;

      std::function<bool(const char*, byte*)> imguiFunc;
      std::function<void(Serializer&, const byte*)> serialize;
      std::function<bool(const Deserializer&, byte*)> deserialize;
   };

   struct ComponentInfo {
      TypeID typeID;
      std::function<void*(Entity&)> tryGet;
      std::function<const void*(const Entity&)> tryGetConst;
      std::function<void*(Entity&)> getOrAdd;
      std::function<void(void*, const void*)> duplicate;
      std::function<void(Entity&, const void*)> copyCtor;
      std::function<void(Entity&, const void*)> moveCtor;

      // ComponentInfo() = default;
      // ComponentInfo(ComponentInfo&&) = default;
      // ONLY_MOVE(ComponentInfo);
   };

   struct ScriptInfo {
      using ApplyFunc = std::function<void(class Script&)>;

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

      void RegisterScript(ScriptInfo&& si);
      void UnregisterScript(TypeID typeID);

      template<typename T>
      const TypeInfo& GetTypeInfo() const {
         return GetTypeInfo(GetTypeID<T>());
      }
      const TypeInfo& GetTypeInfo(TypeID typeID) const;

      template<typename T>
      TypeInfo& GetTypeInfo() {
         return GetTypeInfo(GetTypeID<T>());
      }
      TypeInfo& GetTypeInfo(TypeID typeID);

      template<typename T>
      bool ImGuiValue(std::string_view name, T& value) const {
         auto typeID = GetTypeID<T>();
         return ImGuiValueImpl(name, typeID, (byte*)&value);
      }

      bool ImGuiValueImpl(std::string_view name, TypeID typeID, byte* value) const;

      template<typename T>
      void Serialize(Serializer& ser, std::string_view name, const T& value) const {
         auto typeID = GetTypeID<T>();
         SerializeImpl(ser, name, typeID, (byte*)&value);
      }

      void SerializeImpl(Serializer& ser, std::string_view name, TypeID typeID, const byte* value) const;

      template<typename T>
      bool Deserialize(const Deserializer& deser, std::string_view name, T& value) const {
         auto typeID = GetTypeID<T>();
         return DeserializeImpl(deser, name, typeID, (byte*)&value);
      }

      bool DeserializeImpl(const Deserializer& deser, std::string_view name, TypeID typeID, byte* value) const;

      std::unordered_map<TypeID, TypeInfo> types;

      std::vector<ComponentInfo> components;
      std::vector<ScriptInfo> scripts;
   };

   struct TypeRegisterGuard : RegisterGuardT<decltype([](TypeID typeID) { Typer::Get().UnregisterType(typeID); }) > {
      using RegisterGuardT::RegisterGuardT;
   };

}
