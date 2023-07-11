#include "pch.h"
#include "Script.h"
#include "typer/Typer.h"

#include "scene/Component.h"

namespace pbe {

   void __ScriptUnreg(TypeID typeID) {
      Typer::Get().UnregisterScript(typeID);
   }

   const char* Script::GetName() const {
      return owner.GetName();
   }

}
