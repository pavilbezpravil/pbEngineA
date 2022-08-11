#pragma once
#include "scene/Entity.h"

namespace pbe {

#define TYPER_REGISTER_NATIVE_SCRIPT(Script) \
   static int TyperScriptRegister_##Script() { \
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
      return 0; \
   } \
   static int ScriptInfo_##Script = TyperScriptRegister_##Script()

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
