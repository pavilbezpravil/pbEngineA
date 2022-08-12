#pragma once
#include "core/Type.h"
#include "scene/Entity.h"

namespace pbe {

   struct CORE_API NativeScriptRegisterGuard {
      template<typename Func>
      NativeScriptRegisterGuard(Func f, TypeID typeID) : typeID(typeID) {
         f();
      }
      ~NativeScriptRegisterGuard();
      TypeID typeID;
   };

#define TYPER_REGISTER_NATIVE_SCRIPT(Script) \
   static void TyperScriptRegister_##Script() { \
      auto& typer = Typer::Get(); \
      \
      NativeScriptInfo si{}; \
      si.typeID = GetTypeID<Script>(); \
      \
      si.initialize = [] (Scene& scene) { \
         for (auto [e, script] : scene.GetEntitiesWith<Script>().each()) { \
            script.owner = Entity{e, &scene}; \
         } \
      }; \
      \
      si.sceneApplyFunc = [] (Scene& scene, const NativeScriptInfo::ApplyFunc& func) { \
         for (auto [_, script] : scene.GetEntitiesWith<Script>().each()) { \
            func(script); \
         } \
      }; \
      \
      typer.RegisterNativeScript(std::move(si)); \
   } \
   static NativeScriptRegisterGuard NativeScriptRegisterGuard_##Script = {TyperScriptRegister_##Script, GetTypeID<Script>()}

   class CORE_API NativeScript {
   public:
      virtual ~NativeScript() = default;

      virtual void OnEnable() {}
      virtual void OnDisable() {}

      virtual void OnUpdate(float dt) {}

      const char* GetName() const;

      Entity owner;
   };

}
