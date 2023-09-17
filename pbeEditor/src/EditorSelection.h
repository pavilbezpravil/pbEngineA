#pragma once
#include "scene/Entity.h"

namespace pbe {

   struct EditorSelection {
      void Select(Entity entity, bool clearPrev = true);

      void Unselect(Entity entity);

      void ToggleSelect(Entity entity, bool clearPrev) {
         if (clearPrev) {
            ClearSelection();
         }

         if (IsSelected(entity)) {
            Unselect(entity);
         } else {
            Select(entity, clearPrev);
         }
      }

      // check Input ctrl
      void ToggleSelect(Entity entity);

      bool IsSelected(Entity entity) const {
         auto it = std::ranges::find(selected, entity);
         return it != selected.end();
      }

      bool HasSelection() const {
         return !selected.empty();
      }

      Entity FirstSelected() const {
         return selected.empty() ? Entity{} : selected.front();
      }

      // handle entity destroy
      void SyncWithScene();

      void ClearSelection();

      std::vector<Entity> selected;
   };

}
