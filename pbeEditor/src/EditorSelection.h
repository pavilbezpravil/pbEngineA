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

      Entity FirstSelected() const {
         return selected.empty() ? Entity{} : selected.front();
      }

      void ClearSelection() {
         selected.clear();
      }

      std::vector<Entity> selected;
   };

}
