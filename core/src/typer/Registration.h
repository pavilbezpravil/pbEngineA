#pragma once

#include "Typer.h"


namespace pbe {

   template<typename T>
   concept HasSerialize = requires(T a, Serializer & ser) {
      { a.Serialize(ser) };
   };

   template<typename T>
   concept HasDeserialize = requires(T a, const Deserializer & deser) {
      { a.Deserialize(deser) } -> std::same_as<bool>;
   };

   template<typename T>
   concept HasUI = requires(T a) {
      { a.UI() } -> std::same_as<bool>;
   };

   template<typename T>
   concept HasOnEnable = requires(T a) {
      { a.OnEnable() };
   };

   template<typename T>
   concept HasOnDisable = requires(T a) {
      { a.OnDisable() };
   };

   template<typename T>
   concept HasOnChanged = requires(T a) {
      { a.OnChanged() };
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

   template<typename T>
   auto GetUI() {
      if constexpr (HasUI<T>) {
         return [](const char* lable, byte* data) -> bool { return ((T*)data)->UI(); };
      } else {
         return nullptr;
      }
   }

   template<typename T>
   auto GetOnEnable() {
      if constexpr (HasOnEnable<T>) {
         return [](void* data) { ((T*)data)->OnEnable(); };
      } else {
         return nullptr;
      }
   }

   template<typename T>
   auto GetOnDisable() {
      if constexpr (HasOnDisable<T>) {
         return [](void* data) { ((T*)data)->OnDisable(); };
      } else {
         return nullptr;
      }
   }

   template<typename T>
   auto GetOnChanged() {
      if constexpr (HasOnChanged<T>) {
         return [](void* data) { ((T*)data)->OnChanged(); };
      } else {
         return nullptr;
      }
   }

#define TYPE_REGISTER_GUARD_UNIQUE(unique) \
   static TypeRegisterGuard CONCAT(TypeRegisterGuard_, unique)

#define TYPE_BEGIN(type) \
   TYPE_REGISTER_GUARD_UNIQUE(__LINE__) = \
      {GetTypeID<type>(), [] () { \
         using CurrentType = type; \
         TypeInfo ti; \
         ti.name = #type; \
         ti.typeID = GetTypeID<type>(); \
         ti.typeSizeOf = sizeof(type);

#define TYPE_SERIALIZE(...) \
         ti.serialize = __VA_ARGS__;

#define TYPE_DESERIALIZE(...) \
         ti.deserialize = __VA_ARGS__;

#define TYPE_UI(...) \
         ti.ui = __VA_ARGS__;

#define STRUCT_BEGIN(type) \
   TYPE_BEGIN(type) \
      ti.serialize = GetSerialize<type>(); \
      ti.deserialize = GetDeserialize<type>(); \
      ti.ui = GetUI<type>(); \
      \
      TypeField f{};

   // for handle initialization like this 'TYPER_FIELD_UI2(UISliderFloat{ .min = -10, .max = 15 })'. problem with ','
#define STRUCT_FIELD_UI(...) \
      f.ui = __VA_ARGS__;

#define STRUCT_FIELD(_name) \
      f.name = #_name; \
      f.typeID = GetTypeID<decltype(CurrentType{}._name)>(); \
      f.offset = offsetof(CurrentType, _name); \
      ti.fields.emplace_back(f); \
      f = {};

#define STRUCT_END() \
      Typer::Get().RegisterType(ti.typeID, std::move(ti)); \
   }};

#define ENUM_BEGIN(type) \
   TYPE_BEGIN(type) \
      static std::string enumDescCombo;

#define ENUM_VALUE(Value) \
      CurrentType::Value; \
      enumDescCombo += STRINGIFY(Value); \
      enumDescCombo += '\0';

#define ENUM_END() \
      ti.serialize = [](Serializer& ser, const byte* value) { ser.out << *(int*)value; }; \
      ti.deserialize = [](const Deserializer& deser, byte* value) { *(int*)value = deser.node.as<int>(); return true; }; \
      ti.ui = [](const char* name, byte* value) { return ImGui::Combo(name, (int*)value, enumDescCombo.c_str()); }; \
      Typer::Get().RegisterType(ti.typeID, std::move(ti)); \
   }};

   struct TypeRegisterGuard : RegisterGuardT<decltype([](TypeID typeID) { Typer::Get().UnregisterType(typeID); }) > {
      using RegisterGuardT::RegisterGuardT;
   };

   void __ComponentUnreg(TypeID typeID);

   struct CORE_API ComponentRegisterGuard : RegisterGuardT<decltype([](TypeID typeID) { __ComponentUnreg(typeID); }) > {
      using RegisterGuardT::RegisterGuardT;
   };

#define INTERNAL_ADD_COMPONENT(Component) \
   { \
      static_assert(std::is_move_assignable_v<Component>); \
      ComponentInfo ci{}; \
      ci.typeID = GetTypeID<Component>(); \
      \
      ci.copyCtor = [](Entity& dst, const void* src) { auto srcCompPtr = (Component*)src; return (void*)&dst.Add<Component>((Component&)*srcCompPtr); }; \
      ci.moveCtor = [](Entity& dst, const void* src) { auto srcCompPtr = (Component*)src; return (void*)&dst.Add<Component>((Component&&)*srcCompPtr); }; \
      \
      ci.has = [](const Entity& e) { return e.Has<Component>(); }; \
      ci.add = [](Entity& e) { return (void*)&e.Add<Component>(); }; \
      ci.remove = [](Entity& e) { e.Remove<Component>(); }; \
      ci.get = [](Entity& e) { return (void*)&e.Get<Component>(); }; \
      \
      ci.getOrAdd = [](Entity& e) { return (void*)&e.GetOrAdd<Component>(); }; \
      ci.tryGet = [](Entity& e) { return (void*)e.TryGet<Component>(); }; \
      ci.tryGetConst = [](const Entity& e) { return (const void*)e.TryGet<Component>(); }; \
      \
      ci.duplicate = [](void* dst, const void* src) { *(Component*)dst = *(Component*)src; }; \
      \
      ci.patch = [](Entity& e) { e.MarkComponentUpdated<Component>(); }; \
      ci.onChanged = GetOnChanged<Component>(); \
      \
      typer.RegisterComponent(std::move(ci)); \
   }

#define TYPER_REGISTER_COMPONENT(Component) \
   static void TyperComponentRegister_##Component() { \
      auto& typer = Typer::Get(); \
      ComponentInfo ci{}; \
      INTERNAL_ADD_COMPONENT(Component); \
   } \
   static ComponentRegisterGuard ComponentInfo_##Component = {GetTypeID<Component>(), TyperComponentRegister_##Component};

   void __ScriptUnreg(TypeID typeID);

   struct CORE_API ScriptRegisterGuard : RegisterGuardT<decltype([](TypeID typeID) { __ScriptUnreg(typeID); }) > {
      using RegisterGuardT::RegisterGuardT;
   };

#define TYPER_REGISTER_SCRIPT(Script) \
   static void TyperScriptRegister_##Script() { \
      auto& typer = Typer::Get(); \
      \
      ScriptInfo si{}; \
      si.typeID = GetTypeID<Script>(); \
      \
      si.sceneApplyFunc = [] (Scene& scene, const ScriptInfo::ApplyFunc& func) { \
         for (auto [_, script] : scene.View<Script>().each()) { \
            func(script); \
         } \
      }; \
      \
      typer.RegisterScript(std::move(si)); \
   } \
   static ScriptRegisterGuard ScriptRegisterGuard_##Script = {GetTypeID<Script>(), TyperScriptRegister_##Script}

}
