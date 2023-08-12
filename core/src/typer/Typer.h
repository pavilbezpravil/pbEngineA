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

   template<typename T>
   concept HasSerialize = requires(T a, Serializer & ser) {
      { a.Serialize(ser) };
   };

   template<typename T>
   concept HasDeserialize = requires(T a, const Deserializer & deser) {
      { a.Deserialize(deser) } -> std::same_as<bool>;
   };

   template<typename T>
   auto GetSerialize() {
      if constexpr (HasSerialize<T>) {
         return [](Serializer& ser, const byte* data) { ((T*)data)->Serialize(ser); };
      } else {
         return nullptr;
      }
   }

   template<typename T>
   auto GetDeserialize() {
      if constexpr (HasDeserialize<T>) {
         return [](const Deserializer& deser, byte* data) -> bool { return ((T*)data)->Deserialize(deser); };
      } else {
         return nullptr;
      }
   }

#define TYPER_BEGIN(type) \
   static TypeRegisterGuard TypeRegisterGuard_##type = {GetTypeID<type>(), \
         [] () { \
      using CurrentType = type; \
      TypeInfo ti; \
      ti.name = #type; \
      ti.typeID = GetTypeID<type>(); \
      ti.typeSizeOf = sizeof(type); \
      ti.serialize = GetSerialize<type>(); \
      ti.deserialize = GetDeserialize<type>(); \
      \
      TypeField f{};

#define TYPER_SERIALIZE(...) \
      ti.serialize = __VA_ARGS__;

#define TYPER_DESERIALIZE(...) \
      ti.deserialize = __VA_ARGS__;

#define TYPER_UI(...) \
      ti.imguiFunc = __VA_ARGS__;

   // for handle initialization like this 'TYPER_FIELD_UI2(UISliderFloat{ .min = -10, .max = 15 })'. problem with ','
#define TYPER_FIELD_UI(...) \
      f.uiFunc = __VA_ARGS__;

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
      std::function<void*(Entity&)> add;
      std::function<void(Entity&)> remove;
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

      bool UI(std::string_view name, TypeID typeID, byte* value) const;
      void Serialize(Serializer& ser, std::string_view name, TypeID typeID, const byte* value) const;
      bool Deserialize(const Deserializer& deser, std::string_view name, TypeID typeID, byte* value) const;

      std::unordered_map<TypeID, TypeInfo> types;

      std::vector<ComponentInfo> components;
      std::vector<ScriptInfo> scripts;
   };

   struct TypeRegisterGuard : RegisterGuardT<decltype([](TypeID typeID) { Typer::Get().UnregisterType(typeID); }) > {
      using RegisterGuardT::RegisterGuardT;
   };

}
