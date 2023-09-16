#include "pch.h"
#include "EditorSelection.h"

#include "app/Input.h"
#include "scene/Component.h"

namespace pbe {

   void EditorSelection::Select(Entity entity, bool clearPrev) {
      if (clearPrev) {
         ClearSelection();
      }

      entity.Add<OutlineComponent>();
      selected.emplace_back(entity);
   }

   void EditorSelection::Unselect(Entity entity) {
      auto it = std::ranges::find(selected, entity);
      if (it != selected.end()) {
         if (entity) {
            entity.Remove<OutlineComponent>();
         }
         selected.erase(it);
      }
   }

   void EditorSelection::ToggleSelect(Entity entity) {
      bool clearPrevSelection = !Input::IsKeyPressing(KeyCode::Ctrl);
      ToggleSelect(entity, clearPrevSelection);
   }

   void EditorSelection::ClearSelection() {
      while (!selected.empty()) {
         Unselect(selected.front());
      }
   }

}
