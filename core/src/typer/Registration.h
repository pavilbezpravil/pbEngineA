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

}
