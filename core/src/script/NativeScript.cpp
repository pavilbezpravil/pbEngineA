#include "pch.h"
#include "NativeScript.h"

#include "scene/Component.h"

namespace pbe {

   const char* NativeScript::GetName() const {
      return owner.Get<TagComponent>().tag.data();
   }

}
