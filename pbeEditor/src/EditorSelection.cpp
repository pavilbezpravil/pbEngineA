#include "pch.h"
#include "EditorSelection.h"

#include "app/Input.h"

namespace pbe {

   void EditorSelection::ToggleSelect(Entity entity) {
      bool clearPrevSelection = !Input::IsKeyPressing(VK_CONTROL);
      ToggleSelect(entity, clearPrevSelection);
   }

}
