#include "pch.h"
#include "EditorSelection.h"

#include "app/Input.h"
#include "math/Color.h"
#include "scene/Component.h"

namespace pbe {

   void EditorSelection::Select(Entity entity, bool clearPrev) {
      if (clearPrev) {
         ClearSelection();
      } else {
         Entity parent = entity.GetTransform().parent;
         while (parent) {
            Unselect(parent);
            parent = parent.GetTransform().parent;
         }
      }

      auto IsParentFor = [&] (Entity isParentEntity, Entity isChildEntity) {
         Entity parent = isChildEntity.GetTransform().parent;
         while (parent) {
            if (parent == isParentEntity) {
               return true;
            }
            parent = parent.GetTransform().parent;
         }
         return false;
      };

      bool checkAllSelected = false;
      while (!checkAllSelected) {
         checkAllSelected = true;
         for (auto selectedEntity : selected) {
            if (IsParentFor(entity, selectedEntity)) {
               Unselect(selectedEntity);
               checkAllSelected = false;
               break;
            }
         }
      }

      selected.emplace_back(entity);
   }

   void EditorSelection::Unselect(Entity entity) {
      auto it = std::ranges::find(selected, entity);
      if (it != selected.end()) {
         selected.erase(it);
      }
   }

   void EditorSelection::ToggleSelect(Entity entity) {
      bool clearPrevSelection = !Input::IsKeyPressing(KeyCode::Ctrl);
      ToggleSelect(entity, clearPrevSelection);
   }

   void EditorSelection::SyncWithScene(Scene& scene) {
      // bool unselected = true;
      // while (!unselected) {
      //    unselected = false;
      //    for (auto entity : selected) {
      //       if (!entity) {
      //          Unselect(entity);
      //          unselected = true;
      //          break;
      //       }
      //    }
      // }

      for (auto entity : selected) {
         if (!entity) {
            selected.clear();
            return; // todo:
            Unselect(entity);
         }
      }

      scene.ClearComponent<OutlineComponent>();

      for (auto& entity : selected) {
         entity.Add<OutlineComponent>().color = Color_Yellow;
         AddOutlineForChild(entity, Color_Yellow, 1);
      }
   }

   void EditorSelection::ClearSelection() {
      while (!selected.empty()) {
         Unselect(selected.front());
      }
   }

   void EditorSelection::AddOutlineForChild(Entity& entity, const Color& color, uint depth) {
      Color nextChildColor = color;
      nextChildColor.RbgMultiply(0.8f);

      for (auto& child : entity.GetTransform().children) {
         child.Add<OutlineComponent>().color = color;
         AddOutlineForChild(child, nextChildColor, depth + 1);
      }
   }

}
