#pragma once
#include "scene/Entity.h"

namespace pbe {

   struct EditorSelection {
      void Select(Entity entity, bool clearPrev = true) {
         if (clearPrev) {
            ClearSelection();
         }

         selected.emplace_back(entity);
      }

      void Unselect(Entity entity) {
         auto it = std::ranges::find(selected, entity);
         if (it != selected.end()) {
            selected.erase(it);
         }
      }

      void ToggleSelect(Entity entity, bool clearPrev = true) {
         if (clearPrev) {
            ClearSelection();
         }

         if (IsSelected(entity)) {
            Unselect(entity);
         } else {
            Select(entity, clearPrev);
         }
      }

      bool IsSelected(Entity entity) const {
         auto it = std::ranges::find(selected, entity);
         return it != selected.end();
      }

      Entity FirstSelected() const {
         return selected.empty() ? Entity{} : selected.front();
      }

      void ClearSelection() {
         selected.clear();
      }

      std::vector<Entity> selected;
   };

}
