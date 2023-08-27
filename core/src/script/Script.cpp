#include "pch.h"
#include "Script.h"
#include "scene/Component.h"


namespace pbe {

   const char* Script::GetName() const {
      return owner.GetName();
   }

}
