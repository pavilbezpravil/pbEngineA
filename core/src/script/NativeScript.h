#pragma once
#include "scene/Entity.h"

namespace pbe {

#define TYPER_REGISTER_NATIVE_SCRIPT(Script) \
   static int TyperScriptRegister_##Script() { \
      auto& typer = Typer::Get(); \
      \
      NativeScriptInfo si{}; \
      si.typeID = GetTypeID<Script>(); \
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

   class NativeScript {
   public:
      virtual ~NativeScript() = default;

      virtual void OnUpdate(float dt) {}

      Entity owner;
   };

}
