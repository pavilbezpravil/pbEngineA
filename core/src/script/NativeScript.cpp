#include "pch.h"
#include "NativeScript.h"
#include "typer/Typer.h"

#include "scene/Component.h"

namespace pbe {

   void __ScriptUnreg(TypeID typeID) {
      Typer::Get().UnregisterNativeScript(typeID);
   }

   const char* NativeScript::GetName() const {
      return owner.GetName();
   }

}
