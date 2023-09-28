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

      for (size_t i = selected.size(); i; --i) {
         auto& selectedEntity = selected[i - 1u];
         if (IsParentFor(entity, selectedEntity)) {
            Unselect(selectedEntity);
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

   void EditorSelection::ChangeScene(Scene& scene) {
      for (auto& entity: selected) {
         entity = scene.GetEntity(entity.GetUUID());
      }
   }

   void EditorSelection::SyncWithScene(Scene& scene) {
      for (size_t i = selected.size(); i; --i) {
         auto& entity = selected[i - 1u];
         if (!entity) {
            Unselect(entity);
         }
      }

      scene.ClearComponent<OutlineComponent>();

      for (auto& entity : selected) {
         AddOutlineForChild(entity, Color_Yellow, 0);
      }
   }

   void EditorSelection::ClearSelection() {
      while (!selected.empty()) {
         Unselect(selected.front());
      }
   }

   void EditorSelection::AddOutlineForChild(Entity& entity, const Color& color, uint depth) {
      entity.Add<OutlineComponent>().color = color;

      Color nextChildColor = color;
      nextChildColor.RbgMultiply(0.8f);

      for (auto& child : entity.GetTransform().children) {
         AddOutlineForChild(child, nextChildColor, depth + 1);
      }
   }

}
