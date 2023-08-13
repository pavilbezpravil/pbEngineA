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
      ti.ui = GetUI<type>(); \
      \
      TypeField f{};

#define TYPER_SERIALIZE(...) \
      ti.serialize = __VA_ARGS__;

#define TYPER_DESERIALIZE(...) \
      ti.deserialize = __VA_ARGS__;

#define TYPER_UI(...) \
      ti.ui = __VA_ARGS__;

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
      ci.copyCtor = [](Entity& dst, const void* src) { auto srcCompPtr = (Component*)src; dst.Add<Component>((Component&)*srcCompPtr); }; \
      ci.moveCtor = [](Entity& dst, const void* src) { auto srcCompPtr = (Component*)src; dst.Add<Component>((Component&&)*srcCompPtr); }; \
      \
      ci.has = [](const Entity& e) { return e.Has<Component>(); }; \
      ci.add = [](Entity& e) { return (void*)&e.Add<Component>(); }; \
      ci.remove = [](Entity& e) { e.Remove<Component>(); }; \
      ci.get = [](Entity& e) { e.Get<Component>(); }; \
      \
      ci.getOrAdd = [](Entity& e) { return (void*)&e.GetOrAdd<Component>(); }; \
      ci.tryGet = [](Entity& e) { return (void*)e.TryGet<Component>(); }; \
      ci.tryGetConst = [](const Entity& e) { return (const void*)e.TryGet<Component>(); }; \
      \
      ci.duplicate = [](void* dst, const void* src) { *(Component*)dst = *(Component*)src; }; \
      \
      ci.onEnable = GetOnEnable<Component>(); \
      ci.onDisable = GetOnDisable<Component>(); \
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
