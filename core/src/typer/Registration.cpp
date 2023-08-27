#include "pch.h"
#include "Registration.h"


namespace pbe {

   void __ComponentUnreg(TypeID typeID) {
      Typer::Get().UnregisterComponent(typeID);
   }

   void __ScriptUnreg(TypeID typeID) {
      Typer::Get().UnregisterScript(typeID);
   }

}
